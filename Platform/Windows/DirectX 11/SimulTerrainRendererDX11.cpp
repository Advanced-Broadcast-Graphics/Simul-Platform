// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include "SimulTerrainRendererDX11.h"
#include "Simul/Terrain/Cutout.h"
#include "Simul/Terrain/HeightMapNode.h"

#ifdef XBOX
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static DWORD default_effect_flags=0;
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <D3D11.h>
	#include <d3DX11.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	#define PIXBeginNamedEvent(a,b) D3DPERF_BeginEvent(a,L##b)
	#define PIXEndNamedEvent D3DPERF_EndEvent
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif

#include <stack>
#include "Simul/Base/SmartPtr.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Terrain/Road.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

typedef std::basic_string<TCHAR> tstring;
simul::base::SmartPtr<simul::terrain::HeightMapNode> heightmap;
simul::terrain::HeightMapInterface *heightMapInterface;

SimulTerrainRenderer::SimulTerrainRenderer() :
	m_pVtxDecl(NULL),
	m_pTerrainEffect(NULL),
	terrain_texture(NULL),
	grass_texture(NULL),
	road_texture(NULL),
	vertexBuffer(NULL),
	sky_loss_texture_1(NULL),
	sky_loss_texture_2(NULL),
	sky_inscatter_texture_1(NULL),
	sky_inscatter_texture_2(NULL),
	cloud_textures(NULL),
	skyInterface(NULL)
{
	heightmap=new simul::terrain::HeightMapNode();
	heightmap->SetMaxHeight(4000.f);
	heightmap->SetFractalOctaves(5);
	heightmap->SetFractalScale(60000.f);
	heightmap->SetPageWorldX(120000.f);
	heightmap->SetPageWorldZ(120000.f);
	heightmap->SetPersistence(0.5f);
	heightmap->SetFractalFrequency(64);
	heightmap->Rebuild();
	heightMapInterface=heightmap.get();
	heightmap->SetNumMipMapLevels(1);
}

struct TerrainVertex_t
{
	float x,y,z;
	float normal_x,normal_y,normal_z,ca;
	float tex_x,tex_y;
	float offset;
};

HRESULT SimulTerrainRenderer::Create( ID3D11Device* dev)
{
	m_pd3dDevice=dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	return hr;
}

static float saturate(float f)
{
	if(f<0.f)
		return 0.f;
	if(f>1.f)
		return 1.f;
	return f;
}

void SimulTerrainRenderer::GetVertex(int i,int j,TerrainVertex_t *V)
{
	int grid=heightMapInterface->GetPageSize();
	float X=(float)i/((float)grid-1.f);
	float Y=(float)j/((float)grid-1.f);
	
	V->x=(X-0.5f)*heightMapInterface->GetPageWorldX();
	V->y=heightMapInterface->GetHeightAt(i,j);
	V->z=(Y-0.5f)*heightMapInterface->GetPageWorldZ();
	simul::math::Vector3 n=heightMapInterface->GetNormalAt(i,j);
	float dotp=dir_to_sun*n;
	V->normal_x=n.x;
	V->normal_y=n.z;
	V->normal_z=n.y;
	V->ca=(1.f-saturate((V->y-4400.f)/200.f))*saturate((n.z-0.8f)/0.1f);
	V->tex_x=30.f*X;
	V->tex_y=30.f*Y;
	V->offset=0.f;
}

void SimulTerrainRenderer::GetVertex(float x,float y,TerrainVertex_t *V)
{
	float X=x/heightMapInterface->GetPageWorldX()+0.5f;
	float Y=y/heightMapInterface->GetPageWorldZ()+0.5f;
	
	V->x=x;
	V->y=heightMapInterface->GetHeightAt(x,y);
	V->z=y;
	simul::math::Vector3 n=heightMapInterface->GetNormalAt(x,y);
	float dotp=dir_to_sun*n;
	V->normal_x=n.x;
	V->normal_y=n.z;
	V->normal_z=n.y;
	V->ca=(1.f-saturate((V->y-4400.f)/200.f))*saturate((n.z-0.8f)/0.1f);
	V->tex_x=30.f*X;
	V->tex_y=30.f*Y;
	V->offset=0.f;
}

HRESULT SimulTerrainRenderer::RestoreDeviceObjects( ID3D11Device* dev)
{
	m_pd3dDevice=dev;
	HRESULT hr;
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0,28,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,36,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);

    LPD3DXBUFFER errors=0;
	if(!m_pTerrainEffect)
	{
		if(FAILED(D3DXCreateEffectFromFile(
				m_pd3dDevice,
				TEXT("media\\HLSL\\simul_terrain.fx"),
				NULL,
				NULL,
				default_effect_flags,
				NULL,
				&m_pTerrainEffect,
				&errors)))
		{
			std::string err="";
			if(errors)
			{
				err=static_cast<const char*>(errors->GetBufferPointer());
				std::cerr<<err<<std::endl;
			}
	#ifdef DXTRACE_ERR
			hr=DXTRACE_ERR( L"SimulCloudRenderer::CreateEffect", hr );
	#endif
			DebugBreak();
			return hr;
		}
	}

	SAFE_RELEASE(terrain_texture);
	V_RETURN(hr=D3DXCreateTextureFromFile(m_pd3dDevice,L"Media/Textures/Terrain.dds",&terrain_texture));

	SAFE_RELEASE(grass_texture);
	V_RETURN(hr=D3DXCreateTextureFromFile(m_pd3dDevice,L"Media/Textures/MudGrass01.dds",&grass_texture));

	SAFE_RELEASE(road_texture);
	V_RETURN(hr=D3DXCreateTextureFromFile(m_pd3dDevice,L"Media/Textures/road.dds",&road_texture));

	m_hTechniqueTerrain	=m_pTerrainEffect->GetTechniqueByName("simul_terrain");
	techniqueRoad		=m_pTerrainEffect->GetTechniqueByName("simul_road");
	worldViewProj		=m_pTerrainEffect->GetVariableByName("worldViewProj");
	param_world			=m_pTerrainEffect->GetVariableByName("world");
	eyePosition			=m_pTerrainEffect->GetVariableByName("eyePosition");
	lightDirection		=m_pTerrainEffect->GetVariableByName("lightDir");
	MieRayleighRatio	=m_pTerrainEffect->GetVariableByName("MieRayleighRatio");
	hazeEccentricity	=m_pTerrainEffect->GetVariableByName("HazeEccentricity");
	overcastFactor		=m_pTerrainEffect->GetVariableByName("overcastFactor");
	cloudScales			=m_pTerrainEffect->GetVariableByName("cloudScales");
	cloudOffset			=m_pTerrainEffect->GetVariableByName("cloudOffset");
	cloudInterp			=m_pTerrainEffect->GetVariableByName("cloudInterp");
	fadeInterp			=m_pTerrainEffect->GetVariableByName("fadeInterp");

	lightColour			=m_pTerrainEffect->GetVariableByName("lightColour");
	ambientColour		=m_pTerrainEffect->GetVariableByName("ambientColour");

	g_mainTexture		=m_pTerrainEffect->GetVariableByName("g_mainTexture");
	grassTexture		=m_pTerrainEffect->GetVariableByName("grassTexture");
	roadTexture			=m_pTerrainEffect->GetVariableByName("roadTexture");

	skyLossTexture1		=m_pTerrainEffect->GetVariableByName("skyLossTexture1");
	skyLossTexture2		=m_pTerrainEffect->GetVariableByName("skyLossTexture2");
	skyInscatterTexture1=m_pTerrainEffect->GetVariableByName("skyInscatterTexture1");
	skyInscatterTexture2=m_pTerrainEffect->GetVariableByName("skyInscatterTexture2");
	cloudTexture1		=m_pTerrainEffect->GetVariableByName("cloudTexture1");
	cloudTexture2		=m_pTerrainEffect->GetVariableByName("cloudTexture2");


	int num_vertices=heightMapInterface->GetPageSize()*heightMapInterface->GetPageSize();
	if(skyInterface)
		dir_to_sun=(const float*)skyInterface->GetDirectionToSun();
	int grid=heightMapInterface->GetPageSize();

	SAFE_RELEASE(vertexBuffer);
	V_RETURN(m_pd3dDevice->CreateVertexBuffer( num_vertices*sizeof(TerrainVertex_t),0,0,
									  D3DPOOL_DEFAULT, &vertexBuffer,
									  NULL ));
	TerrainVertex_t *vertices;
	V_RETURN(vertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
	TerrainVertex_t *V=vertices;
	for(int i=0;i<grid;i++)
	{
		for(int j=0;j<grid;j++)
		{
			GetVertex(i,j,V);
			V++;
		}
	}
    V_RETURN(vertexBuffer->Unlock());

	ReleaseIndexBuffers();

//	BuildRoad();

	int squares=heightMapInterface->GetTileSize()-1;
	size_t tile_grid=(heightMapInterface->GetPageSize()-1)/squares;
	tiles.resize(tile_grid);
	for(size_t i=0;i<tile_grid;i++)
	{
		for(size_t j=0;j<tile_grid;j++)
		{
			tiles[i].push_back(TerrainTile(squares,heightMapInterface->GetNumMipMapLevels()));
			for(int m=0;m<heightMapInterface->GetNumMipMapLevels();m++)
				BuildTile(&(tiles[i][j]),(int)i,(int)j,m);
		}
	}
		
	return hr;
}


HRESULT SimulTerrainRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pTerrainEffect)
        hr=m_pTerrainEffect->OnLostDevice();
	SAFE_RELEASE(m_pTerrainEffect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(vertexBuffer);
	ReleaseIndexBuffers();
	SAFE_RELEASE(terrain_texture);
	SAFE_RELEASE(grass_texture);
	SAFE_RELEASE(road_texture);
	sky_loss_texture_1=NULL;
	sky_loss_texture_2=NULL;
	sky_inscatter_texture_1=NULL;
	sky_inscatter_texture_2=NULL;
	cloud_textures=NULL;
	return hr;
}

HRESULT SimulTerrainRenderer::Destroy()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(vertexBuffer);
	ReleaseIndexBuffers();
	SAFE_RELEASE(m_pTerrainEffect);
	SAFE_RELEASE(terrain_texture);
	SAFE_RELEASE(grass_texture);
	SAFE_RELEASE(road_texture);
	sky_loss_texture_1=NULL;
	sky_loss_texture_2=NULL;
	sky_inscatter_texture_1=NULL;
	sky_inscatter_texture_2=NULL;
	cloud_textures=NULL;
	return hr;
}

SimulTerrainRenderer::~SimulTerrainRenderer()
{
	Destroy();
}
	static const float radius=50.f;
	static const float height=150.f;

HRESULT SimulTerrainRenderer::Render()
{
	HRESULT hr=S_OK;
	PIXBeginNamedEvent(0,"Render Terrain");
	
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);

	m_pTerrainEffect->SetTechnique( m_hTechniqueTerrain );

	D3DXMATRIX tmp1, tmp2,wvp;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&wvp,&tmp2);
	m_pTerrainEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&wvp));
	m_pTerrainEffect->SetMatrix(param_world,(const D3DXMATRIX *)(&world));
	m_pTerrainEffect->SetVector(eyePosition,(const D3DXVECTOR4 *)(&cam_pos));

	if(skyInterface)
	{
		D3DXVECTOR4 mie_rayleigh_ratio(skyInterface->GetMieRayleighRatio());
		D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToSun());
	//if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);

		m_pTerrainEffect->SetVector	(lightDirection		,&sun_dir);
		m_pTerrainEffect->SetVector	(MieRayleighRatio	,&mie_rayleigh_ratio);
		m_pTerrainEffect->AsScalar()->SetFloat	(hazeEccentricity	,skyInterface->GetMieEccentricity());
		float alt_km=cam_pos.y*0.001f;
		static float light_mult=0.17f/6.28f;
		simul::sky::float4 light_colour=light_mult*skyInterface->GetLocalIrradiance(alt_km);
		simul::sky::float4 ambient_colour=skyInterface->GetAmbientLight(alt_km);
		m_pTerrainEffect->SetVector	(lightColour		,(const D3DXVECTOR4 *)(&light_colour));
		m_pTerrainEffect->SetVector	(ambientColour		,(const D3DXVECTOR4 *)(&ambient_colour));
	}
	m_pTerrainEffect->AsScalar()->SetFloat	(overcastFactor		,overcast_factor);
	m_pTerrainEffect->SetVector	(cloudScales		,(const D3DXVECTOR4 *)(cloud_scales));
	m_pTerrainEffect->SetVector	(cloudOffset		,(const D3DXVECTOR4 *)(cloud_offset));
	m_pTerrainEffect->AsScalar()->SetFloat	(cloudInterp		,cloud_interp);
	m_pTerrainEffect->AsScalar()->SetFloat	(fadeInterp			,fade_interp);

	m_pTerrainEffect->SetTexture(g_mainTexture		,terrain_texture);
	m_pTerrainEffect->SetTexture(grassTexture		,grass_texture);
	m_pTerrainEffect->SetTexture(skyLossTexture1	,sky_loss_texture_1);
	m_pTerrainEffect->SetTexture(skyLossTexture2	,sky_loss_texture_2);
	m_pTerrainEffect->SetTexture(skyInscatterTexture1,sky_inscatter_texture_1);
	m_pTerrainEffect->SetTexture(skyInscatterTexture2,sky_inscatter_texture_2);
	if(cloud_textures)
	{
		m_pTerrainEffect->SetTexture(cloudTexture1		,cloud_textures[0]);
		m_pTerrainEffect->SetTexture(cloudTexture2		,cloud_textures[1]);
	}

	UINT passes=1;
	hr=m_pTerrainEffect->Begin( &passes, 0 );

	V_RETURN(hr=m_pd3dDevice->SetStreamSource(0,
			vertexBuffer,
			0,
			sizeof(TerrainVertex_t)
			));

	V_RETURN(hr=m_pd3dDevice->IASetInputLayout( m_pVtxDecl ));
	for(unsigned p = 0 ; p < passes ; ++p )
	{
		hr=m_pTerrainEffect->BeginPass(p);

		for(size_t i=0;i<tiles.size();i++)
		{
			for(size_t j=0;j<tiles.size();j++)
			{
				int mip_level=2;
				simul::math::Vector3 pos=tiles[i][j].pos;
				//std::swap(pos.y,pos.z);
				pos-=simul::math::Vector3((const float*)(&cam_pos));
				float dist=pos.Magnitude();
				mip_level=(int)(dist/12000.f);
				if(mip_level>heightMapInterface->GetNumMipMapLevels()-1)
					mip_level=heightMapInterface->GetNumMipMapLevels()-1;
				TerrainTileMIP *mip=&tiles[i][j].mips[mip_level];
				V_RETURN(hr=m_pd3dDevice->SetIndices(mip->indexBuffer));

				if(mip->tri_strip)
					V_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,mip->num_verts,0,mip->num_prims-2))
				else if(mip->num_squares)
					V_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,mip->num_verts,0,mip->num_prims/3))

				if(mip->extraIndexBuffer)
				{
					V_RETURN(hr=m_pd3dDevice->SetStreamSource(0,mip->extraVertexBuffer,0,sizeof(TerrainVertex_t)));
					V_RETURN(hr=m_pd3dDevice->SetIndices(mip->extraIndexBuffer));
				V_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,mip->extra_vertices.size(),0,mip->extra_triangles.size()/3))
					V_RETURN(hr=m_pd3dDevice->SetStreamSource(0,vertexBuffer,0,sizeof(TerrainVertex_t)));
				}
			}
		}

		hr=m_pTerrainEffect->EndPass();
	}
	hr=m_pTerrainEffect->End();

	m_pTerrainEffect->SetTechnique( techniqueRoad );
	m_pTerrainEffect->SetTexture(g_mainTexture,road_texture);
	hr=m_pTerrainEffect->Begin( &passes, 0 );
	for(unsigned p = 0 ; p < passes ; ++p )
	{
		hr=m_pTerrainEffect->BeginPass(p);
		for(size_t i=0;i<roads.size();i++)
		{
			if(roads[i].roadVertexBuffer)
			{
				V_RETURN(hr=m_pd3dDevice->SetStreamSource(0,roads[i].roadVertexBuffer,0,sizeof(TerrainVertex_t)));
				V_RETURN(hr=m_pd3dDevice->SetIndices(roads[i].roadIndexBuffer));
				V_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,roads[i].num_verts,0,roads[i].num_verts-2))
				V_RETURN(hr=m_pd3dDevice->SetStreamSource(0,vertexBuffer,0,sizeof(TerrainVertex_t)));
			}
		}
		hr=m_pTerrainEffect->EndPass();
	}
	hr=m_pTerrainEffect->End();

	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr=m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	D3DXMatrixIdentity(&world);
	PIXEndNamedEvent();
	return hr;
}

void SimulTerrainRenderer::SetMatrices(const D3DXMATRIX &w,const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	world=w;
	view=v;
	proj=p;
}
simul::terrain::HeightMapInterface *SimulTerrainRenderer::GetHeightMapInterface()
{
	return heightMapInterface;
}

void SimulTerrainRenderer::Update(float )
{
}

void SimulTerrainRenderer::ReleaseIndexBuffers()
{
	for(size_t i=0;i<tiles.size();i++)
	{
		for(size_t j=0;j<tiles.size();j++)
		{
			tiles[i][j].Reset();
		}
	}
	for(size_t i=0;i<roads.size();i++)
	{
		roads[i].Reset();
	}
}

int SimulTerrainRenderer::TilePosToIndex(int i,int j,int x,int y) const
{
	int grid=heightMapInterface->GetPageSize();
	int tile_size=heightMapInterface->GetTileSize();
	int I=i*(tile_size-1)+x;
	int J=j*(tile_size-1)+y;
	return I*(grid)+J;
}
simul::base::SmartPtr<simul::terrain::Road> road;
	
HRESULT SimulTerrainRenderer::BuildRoad()
{
	HRESULT hr=S_OK;
	road=new simul::terrain::Road(heightMapInterface);

	roads.push_back(RoadRenderable());
	RoadRenderable *rr=&(roads.back());
	rr->num_verts=2*road->GetNumSegments()*12;

	
	SAFE_RELEASE(rr->roadVertexBuffer);
	V_RETURN(m_pd3dDevice->CreateVertexBuffer( rr->num_verts*sizeof(TerrainVertex_t),0,0,
									  D3DPOOL_DEFAULT, &rr->roadVertexBuffer,
									  NULL ));
	TerrainVertex_t *vertices;
	V_RETURN(rr->roadVertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
	TerrainVertex_t *V=vertices;

	//RHS of road
	float along=0.f;
	float tex_repeat_length=50.f;
	for(int a=0;a<rr->num_verts/2;a++)
	{
		simul::math::float3 x=road->GetCentreLine ((float)a/(float)(rr->num_verts/2-1));
		simul::math::float3 nextx
							=road->GetCentreLine ((float)(a+1)/(float)(rr->num_verts/2-1));
		simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(rr->num_verts/2-1));
		simul::math::float2 left ((const float*)(x+dx));
		simul::math::float2 right((const float*)(x-dx));
		GetVertex(right.x,right.y,V);
		V->y=x.z;
		V->tex_x=0.f;
		V->tex_y=along/tex_repeat_length;
		V++;
		GetVertex( left.x, left.y,V);
		V->y=x.z;
		V->tex_x=1.f;
		V->tex_y=along/tex_repeat_length;
		V++;
		along+=simul::math::float3::length(nextx-x);
	}
    V_RETURN(rr->roadVertexBuffer->Unlock());
	
	V_RETURN(m_pd3dDevice->CreateIndexBuffer(rr->num_verts*sizeof(unsigned),0,DXGI_FORMAT_INDEX32,
		D3DPOOL_DEFAULT, &rr->roadIndexBuffer, NULL ));
	unsigned *indexData;
	V_RETURN(rr->roadIndexBuffer->Lock(0, rr->num_verts, (void**)&indexData, 0 ));

	unsigned *index=indexData;
	for(int x=0;x<rr->num_verts/2;x++)
	{
		*(index++)	=2*x;
		*(index++)	=2*x+1;
	}
	V_RETURN(rr->roadIndexBuffer->Unlock());
	return hr;
}

HRESULT SimulTerrainRenderer::BuildTile(TerrainTile *tile,int i,int j,int mip_level)
{
	int tile_size=heightMapInterface->GetTileSize();
	int skip=1<<mip_level;


	float X=((float)i+0.5f)/((float)tiles.size());
	float Y=((float)j+0.5f)/((float)tiles.size());
	simul::math::Vector3 pos;
	tile->pos[0]=(X-0.5f)*heightMapInterface->GetPageWorldX();
	tile->pos[1]=heightMapInterface->GetHeightAt(i*(tile_size-1)+tile_size/2,j*(tile_size-1)+tile_size/2);
	tile->pos[2]=(Y-0.5f)*heightMapInterface->GetPageWorldZ();
	

	TerrainTileMIP *mip=&(tile->mips[mip_level]);

	mip->num_verts=heightMapInterface->GetPageSize()*heightMapInterface->GetPageSize();
	simul::terrain::Cutout cutout;
#if 1
	//const float *r=road->GetSegmentData();
	if(road)
	{
		int s=12*road->GetNumSegments();
		simul::math::float2 dx;

		//RHS of road
		for(int a=0;a<s;a++)
		{
			simul::math::float3 x=road->GetCentreLine((float)a/(float)(s-1));
			simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(s-1));
			cutout.addvertex((const float*)(x+dx),x.z);
		}
		//LHS of road:
		for(int a=s-1;a>=0;a--)
		{
			simul::math::float3 x=road->GetCentreLine((float)a/(float)(s-1));
			simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(s-1));
			cutout.addvertex((const float*)(x-dx),x.z);
		}
	}
#else
	float rad=4500.f;
	for(int a=0;a<16;a++)
	{
		float angle=2.f*3.14159f*(float)a/(float)16;
		cutout.addvertex(simul::math::float2(rad*cos(angle),-rad*sin(angle)),0);
	}
#endif

	mip->ApplyCutouts(heightMapInterface,i,j,skip,&cutout);


	HRESULT hr=S_OK;
//	int grid=heightMapInterface->GetPageSize();
	tile_size-=1;
	tile_size>>=mip_level;
	tile_size+=1;
	if(mip->tri_strip)
	{
		mip->num_prims=(tile_size+1)*2*(tile_size-1);
		V_RETURN(m_pd3dDevice->CreateIndexBuffer(mip->num_prims*sizeof(unsigned),0,DXGI_FORMAT_INDEX32,
										  D3DPOOL_DEFAULT, &mip->indexBuffer,
										  NULL ));
		unsigned *indexData;
		V_RETURN(mip->indexBuffer->Lock(0, mip->num_prims, (void**)&indexData, 0 ));

		unsigned *index=indexData;
		for(int x=0;x<tile_size-1;x+=2)
		{
			for(int y=0;y<tile_size;y++)
			{
				*(index++)	=TilePosToIndex(i,j,(x+1)*skip,y*skip);
				*(index++)	=TilePosToIndex(i,j,(x  )*skip,y*skip);
			}
			*(index++)	=TilePosToIndex(i,j,(x  )*skip,(tile_size-1)*skip);
			for(int y=0;y<tile_size;y++)
			{
				*(index++)	=TilePosToIndex(i,j,(x+2)*skip,(tile_size-1-y)*skip);
				*(index++)	=TilePosToIndex(i,j,(x+1)*skip,(tile_size-1-y)*skip);
			}
			*(index++)	=TilePosToIndex(i,j,(x+1)*skip,0);
		}
		V_RETURN(mip->indexBuffer->Unlock());
	}
	else
	{
		if(mip->num_squares)
		{
		// Must create 
			mip->num_prims=6*mip->num_squares;
			V_RETURN(m_pd3dDevice->CreateIndexBuffer((unsigned)(mip->num_prims*sizeof(unsigned)),0,DXGI_FORMAT_INDEX32,
											  D3DPOOL_DEFAULT, &mip->indexBuffer,
											  NULL ));
			unsigned *indexData;
			V_RETURN(mip->indexBuffer->Lock(0, mip->num_prims, (void**)&indexData, 0 ));
			unsigned *index=indexData;
			for(int x=0;x<tile_size-1;x++)
			{
				for(int y=0;y<tile_size-1;y++)
				{
					if(!mip->unimpeded_squares[x][y])
						continue;
					*(index++)	=TilePosToIndex(i,j,(x  )*skip,y*skip);
					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,(y+1)*skip);
					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,y*skip);

					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,(y+1)*skip);
					*(index++)	=TilePosToIndex(i,j,(x  )*skip,y*skip);
					*(index++)	=TilePosToIndex(i,j,(x  )*skip,(y+1)*skip);
				}
			}
			V_RETURN(mip->indexBuffer->Unlock());
		}

		if(mip->extra_vertices.size())
		{
			// Must create own vertex buffer:
			SAFE_RELEASE(mip->extraVertexBuffer);
			V_RETURN(m_pd3dDevice->CreateVertexBuffer((unsigned)(mip->extra_vertices.size()*sizeof(TerrainVertex_t)),
											0,0,
										  D3DPOOL_DEFAULT, &mip->extraVertexBuffer,
										  NULL ));
			TerrainVertex_t *vertices;
			V_RETURN(mip->extraVertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
			TerrainVertex_t *V=vertices;
			for(size_t i=0;i<mip->extra_vertices.size();i++)
			{
				GetVertex(mip->extra_vertices[i].x,mip->extra_vertices[i].y,V);
				V->y=mip->extra_vertices[i].z;
				V++;
			}
			V_RETURN(mip->extraVertexBuffer->Unlock());
		}
		if(mip->extra_triangles.size())
		{
			// and the index buffer for these extra vertices:
			V_RETURN(m_pd3dDevice->CreateIndexBuffer((unsigned)(mip->extra_triangles.size()*sizeof(unsigned)),0,DXGI_FORMAT_INDEX32,
											  D3DPOOL_DEFAULT, &mip->extraIndexBuffer,
											  NULL ));
			unsigned *indexData;
			V_RETURN(mip->extraIndexBuffer->Lock(0,(unsigned)mip->extra_triangles.size(), (void**)&indexData, 0 ));
			unsigned *index=indexData;
			for(size_t i=0;i<mip->extra_triangles.size();i++)
			{
				*(index++)	=mip->extra_triangles[i];
			}
			V_RETURN(mip->extraIndexBuffer->Unlock());
		}
	}
	return S_OK;
}

SimulTerrainRenderer::TerrainTile::TerrainTile(int tilesize,int num_mips)
{
	for(int i=0;i<num_mips;i++)
	{
		mips.push_back(TerrainTileMIP(tilesize));
		tilesize>>=1;
	}
}

SimulTerrainRenderer::TerrainTileMIP::TerrainTileMIP(int tilesize)
	:indexBuffer(NULL)
	,extraVertexBuffer(NULL)
	,extraIndexBuffer(NULL)
	,num_prims(0)
{
	tile_size=tilesize;
	unimpeded_squares.resize(tilesize);
	num_squares=0;
	tri_strip=false;
	for(int i=0;i<tilesize;i++)
	{
		unimpeded_squares[i].resize(tilesize);
	}
}

void SimulTerrainRenderer::TerrainTileMIP::Reset()
{
	SAFE_RELEASE(indexBuffer);
	SAFE_RELEASE(extraVertexBuffer);
	SAFE_RELEASE(extraIndexBuffer);
	indexBuffer=NULL;
	num_prims=0;
	num_squares=0;
	num_verts=0;
	tri_strip=false;
}

void SimulTerrainRenderer::RoadRenderable::Reset()
{
	SAFE_RELEASE(roadVertexBuffer);
	SAFE_RELEASE(roadIndexBuffer);
	roadVertexBuffer=NULL;
	roadIndexBuffer=NULL;
	num_verts=0;
}

static size_t loopindex(int idx,size_t length)
{
	while(idx<0)
		idx+=length;
	return (size_t)(idx%(length));
}
// which squares are affected?
void SimulTerrainRenderer::TerrainTileMIP::ApplyCutouts(
	simul::terrain::HeightMapInterface *hmi,int i,int j,int step,const simul::terrain::Cutout *cutout)
{
	simul::math::float2 min_corner,max_corner;
	cutout->GetCorners(min_corner,max_corner);
	
	simul::math::float2 block_min,block_max;

	hmi->GetBlockCorners(i,j,block_min,block_max);

	if(!(block_min<max_corner&&block_max>min_corner)
		)
	{
		num_squares=0;
		tri_strip=true;
		for(int x=0;x<tile_size;x++)
		{
			for(int y=0;y<tile_size;y++)
			{
				this->unimpeded_squares[x][y]=true;
			}
		}
		return;
	}
	tri_strip=false;
	num_squares=0;
	//simul::math::float2 block_size=block_max-block_min;
	// test each square.
	//simul::math::float2 square_size=block_size/(float)(hmi->GetTileSize()-1);
	for(int x=0;x<tile_size;x++)
	{
		for(int y=0;y<tile_size;y++)
		{
			this->unimpeded_squares[x][y]=true;
			simul::math::float2 square_min,square_max;
			hmi->GetVertexPos((i*tile_size+x)*step,(j*tile_size+y)*step,square_min);
			hmi->GetVertexPos((i*tile_size+x+1)*step,(j*tile_size+y+1)*step,square_max);
			if(square_min.x>max_corner.x||square_min.y>max_corner.y
				||square_max.x<min_corner.x||square_max.y<min_corner.y
				)
			{
				num_squares++;
				continue;
			}
			this->unimpeded_squares[x][y]=false;
			struct State
			{
				simul::math::float2 this_point,next_point;
				int Case;
				size_t corner;
				size_t index;
				int counter;
				simul::terrain::Cutout::Corner start_corner;
			};
			std::stack<State> remaining;
			std::stack<simul::terrain::Cutout> polygons;
			State state;
			simul::terrain::Cutout square;
			square.addvertex(square_min,hmi->GetHeightAt(square_min.x,square_min.y));
			square.addvertex(simul::math::float2(square_min.x,square_max.y),hmi->GetHeightAt(square_min.x,square_max.y));
			square.addvertex(square_max,hmi->GetHeightAt(square_max.x,square_max.y));
			square.addvertex(simul::math::float2(square_max.x,square_min.y),hmi->GetHeightAt(square_max.x,square_min.y));
			simul::terrain::Cutout building;
			state.Case=0;		// going along the edge of the square outside the cutout
			// firstly is the bottom-left corner inside or outside of the cutout?
			state.counter=0;
			state.start_corner=NULL;
			if(cutout->IsInside(square_min))
			{	
				this->unimpeded_squares[x][y]=false;
				state.counter=1;
				state.Case=1;		// going along the edge of the square inside the cutout
			}
			else	// add a vertex:
			{
				building.addvertex(square_min,hmi->GetHeightAt(square_min.x,square_min.y));
				state.start_corner=simul::terrain::Cutout::Corner(&square.vertex(0),&square.vertex(-1));
			}
			state.corner=0;
			state.index=0;
			state.this_point=square.vertex(state.corner).pos;
			state.next_point=square.vertex(state.corner+1).pos;
			int direction=1;// anticlockwise.
			cutout->unuse();
			while(state.corner<4)
			{
				simul::math::float2 intersect;
				// go round the square clockwise, looking for intersections.
				// crossover=1 means entering - use the anti-clockwise direction, 1 means clockwise.
				int edge=-1;
				int crossover=0;
				bool finished=false;
				if(state.Case==0)
				{
					square.unuse();
					square.vertex(state.index).used=true;
					crossover=cutout->GetCrossover(state.this_point,state.next_point,intersect,edge);
					if(crossover==0)
					{
						cutout->unuse();
						state.corner++;
						state.index++;
						if(state.corner<4)
							building.addvertex(state.next_point,hmi->GetHeightAt(state.next_point.x,state.next_point.y));
						state.this_point=state.next_point;
						state.next_point=square.vertex(state.corner+1).pos;
					}
					// we've entered the polygon. 
					else if(!state.counter&&crossover==1)
					{
						simul::terrain::Cutout::Corner this_corner(&cutout->vertex(edge),&square.vertex(state.corner));
						// was this where we started?
						if(state.start_corner==this_corner)
						{
							finished=true;
						}
						else
						{
							// The state branches here. The first branch moves along the outside of the cutout.
							// The second branch moves on the square in State 1.
							{
								remaining.push(State());
								State &branch=remaining.top();
								branch.this_point=intersect;
								branch.next_point=state.next_point;
								branch.Case=1;
								branch.counter=1;
								branch.corner=state.corner;
								branch.index=branch.corner;
								branch.start_corner=this_corner;

							}
							{
								building.addvertex(intersect,cutout->GetHeightAt(edge,intersect));
								state.index=edge;
								if(crossover>0)
									state.index++;
								direction=-crossover;
								cutout->unuse();
								cutout->vertex(state.index-((direction<0)?1:0)).used=true;
								state.this_point=intersect;
								if(!state.counter)
								{
									state.Case=2;		// going along the edge of the cutout
									// next point along cutout.
									state.next_point=cutout->vertex(state.index+direction).pos;
								}
							}
						}
						unimpeded_squares[x][y]=false;
						state.counter++;
					}
					else
					{
						//ignore this edge:
						cutout->vertex(edge).used=true;
					}
				}
				else if(state.Case==2)
				{
					// do we hit an edge of the square?
					crossover=square.GetCrossover(state.this_point,state.next_point,intersect,edge);
					square.unuse();
					if(crossover==0)
					{
						building.addvertex(state.next_point,cutout->GetHeightAt(state.index-(direction<0),state.next_point));
						state.index+=direction;
						cutout->unuse();
						cutout->vertex(state.index-((direction<0)?1:0)).used=true;
						state.this_point=state.next_point;
						state.next_point=cutout->vertex(state.index+direction).pos;
					}
					else if(state.counter&&crossover==-1)
					{
						if(edge<state.corner)
							edge+=4;
						state.this_point=intersect;
						state.counter--;
						if(!state.counter)
						{
							simul::terrain::Cutout::Corner this_corner(&cutout->vertex(state.index-(direction<0)),&square.vertex(edge));
							// was this where we started?
							if(state.start_corner==this_corner)
							{
								finished=true;
							}
							state.corner=edge;
							if(state.corner<4)
								building.addvertex(intersect,cutout->GetHeightAt(state.index-(direction<0),intersect));
							state.Case=0;		// going along the edge of the square
							state.next_point=square.vertex(edge-crossover).pos;	// next point along cutout.
						}
						else
							building.addvertex(intersect,cutout->GetHeightAt(state.index,intersect));
						state.index=edge;
						direction=1;
					}
					else
						DebugBreak();
				}
				else //Case=1
				{
					square.vertex(state.index).used=true;
					crossover=cutout->GetCrossover(state.this_point,state.next_point,intersect,edge);
					square.unuse();
					if(crossover==0)
					{
						cutout->unuse();
						state.corner++;
						state.index++;
						state.this_point=state.next_point;
						state.next_point=square.vertex(state.corner+1).pos;
					}
					// we've exited the polygon. 
					else if(state.counter&&crossover==-1)
					{
						simul::terrain::Cutout::Corner this_corner(&cutout->vertex(edge),&square.vertex(state.index));
						building.addvertex(intersect,cutout->GetHeightAt(edge,intersect));
						cutout->unuse();
						cutout->vertex(edge).used=true;
						direction=1;
						state.this_point=intersect;
						state.counter--;
						if(!state.counter)
						{
							state.Case=0;		// going along the edge of the square
							state.start_corner=this_corner;
							
						}
						this->unimpeded_squares[x][y]=false;
					}
					else
					{
						//ignore this edge:
						cutout->vertex(edge).used=true;
					}
				}
				if(state.corner>=4||finished)
				{
					polygons.push(building);
					building.vertices.clear();
					if(remaining.size())
					{
						state=remaining.top();
						remaining.pop();
					}
				}
			}
			if(unimpeded_squares[x][y])
				num_squares++;
			else
			{
				while(polygons.size())
				{
					simul::terrain::Cutout& building=polygons.top();
					// eliminate close duplicates
					for(size_t i=0;i<building.vertices.size();i++)
					{
						simul::math::float2 diff=building.vertex(i).pos-building.vertex(i+1).pos;
						// merge values that are less than 1mm apart:
						if(simul::math::float2::length(diff)<0.1f)
						{
						//	building.vertices.erase(building.vertices.begin()+i);
						//	i--;
						}
					}
					std::vector<int> indices;
					for(size_t i=0;i<building.vertices.size();i++)
					{
						indices.push_back(extra_vertices.size());
						extra_vertices.push_back(simul::math::float3(
							building.vertex(i).pos.x,building.vertex(i).pos.y,
							building.vertex(i).height));
					}
					//triangulate the "building" polygon:
					while(building.vertices.size()>=3)
					{
						int vert_ear=building.FindEar();
						if(vert_ear<0)
						{
							building.FindEar();
							break;
						}
						// the new triangle is between vert_ear and the two adjacent vertices.
						extra_triangles.push_back(indices[vert_ear]);
						extra_triangles.push_back(indices[loopindex(vert_ear+1,indices.size())]);
						extra_triangles.push_back(indices[loopindex(vert_ear-1,indices.size())]);
						building.vertices.erase(building.vertices.begin()+vert_ear);
						indices.erase(indices.begin()+vert_ear);
						
					}
					if(building.vertices.size()==3)
					{
						for(int i=0;i<building.vertices.size();i++)
						{
							extra_triangles.push_back(indices[i]);
						}
					}
					else
					{
					}
					polygons.pop();
				}
			}
		}
	}
}

void SimulTerrainRenderer::TerrainTile::Reset()
{
	for(size_t i=0;i<mips.size();i++)
	{
		mips[i].Reset();
	}
}