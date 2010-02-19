// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulSkyRenderer.cpp A renderer for skies.

#include "SimulSkyRenderer.h"

#ifdef XBOX
	#include <dxerr9.h>
	#include <string>
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <d3d9.h>
	#include <d3dx9.h>
	#include <dxerr9.h>
	#include <string>
	static DWORD default_effect_flags=D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif

#include "CreateDX9Effect.h"
#include "Macros.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/SkyNode.h"
#include "Simul/Sky/AltitudeFadeTable.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Resources.h"

SimulSkyRenderer::SimulSkyRenderer() :
	skyTexSize(0),
	fadeTexWidth(0),
	fadeTexHeight(0),
	skyTexIndex(0),
	numAltitudes(0),
	overcast_factor(0.f),
	m_pVtxDecl(NULL),
	m_pSkyEffect(NULL),
	m_hTechniqueSky(NULL),	
	m_hTechniqueSun(NULL),
	m_hTechniqueQuery(NULL),	
	m_hTechniqueFlare(NULL),	
	m_hTechniquePlanet(NULL),
	skyInterface(NULL),
	flare_texture(NULL),
	moon_texture(NULL),
	sun_occlusion(0.f),
	d3dQuery(NULL),
	sky_tex_format(D3DFMT_UNKNOWN)
{
	for(int i=0;i<3;i++)
	{
		sky_textures[i]=NULL;
		loss_textures[i]=NULL;
		inscatter_textures[i]=NULL;
		loss_textures_3d[i]=NULL;
		inscatter_textures_3d[i]=NULL;
	}
	skyNode=new simul::sky::SkyNode();
	skyInterface=skyNode.get();
	skyNode->SetMieWavelengthExponent(0.f);
	fadeTable=new simul::sky::AltitudeFadeTable(skyInterface,0,128,128,32,200.f);
	fadeTable->SetEarthTest(true);
	skyNode->SetTimeMultiplier(1.f);
	skyNode->SetHourOfTheDay(11.f);
	skyNode->SetSunIrradiance(simul::sky::float4(25,25,25,0));
	skyNode->SetHazeScaleHeightKm(2.f);
	skyNode->SetHazeBaseHeightKm(4.f);
	cam_pos.x=cam_pos.z=0;
	cam_pos.y=400.f;
}

void SimulSkyRenderer::SetStepsPerDay(unsigned steps)
{
	fadeTable->SetStepsPerDay((float)steps);
}

simul::sky::SkyInterface *SimulSkyRenderer::GetSkyInterface()
{
	return skyInterface;
}

simul::sky::FadeTableInterface *SimulSkyRenderer::GetFadeTableInterface()
{
	return fadeTable.get();
}

HRESULT SimulSkyRenderer::RestoreDeviceObjects( LPDIRECT3DDEVICE9 dev)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
#ifndef XBOX
	sky_tex_format=D3DFMT_A16B16G16R16F;
#else
	sky_tex_format=D3DFMT_LIN_A16B16G16R16F;
#endif
	hr=CanUse16BitFloats(m_pd3dDevice);

	if(hr!=S_OK)
	{
#ifndef XBOX
		sky_tex_format=D3DFMT_A32B32G32R32F;
#else
		sky_tex_format=D3DFMT_LIN_A32B32G32R32F;
#endif
		V_RETURN(CanUseTexFormat(m_pd3dDevice,sky_tex_format));
	}

    m_pd3dDevice->CreateQuery( D3DQUERYTYPE_OCCLUSION, &d3dQuery );
	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);
	SAFE_RELEASE(m_pSkyEffect);
	hr=CreateSkyEffect();
	m_hTechniqueSky		=m_pSkyEffect->GetTechniqueByName("simul_sky");
	worldViewProj		=m_pSkyEffect->GetParameterByName(NULL,"worldViewProj");
	lightDirection		=m_pSkyEffect->GetParameterByName(NULL,"lightDir");
	MieRayleighRatio	=m_pSkyEffect->GetParameterByName(NULL,"MieRayleighRatio");
	hazeEccentricity	=m_pSkyEffect->GetParameterByName(NULL,"HazeEccentricity");
	overcastFactor		=m_pSkyEffect->GetParameterByName(NULL,"overcastFactor");
	skyInterp			=m_pSkyEffect->GetParameterByName(NULL,"skyInterp");
	altitudeTexCoord	=m_pSkyEffect->GetParameterByName(NULL,"altitudeTexCoord");
	
	colour				=m_pSkyEffect->GetParameterByName(NULL,"colour");
	m_hTechniqueSun		=m_pSkyEffect->GetTechniqueByName("simul_sun");
	m_hTechniqueFlare	=m_pSkyEffect->GetTechniqueByName("simul_flare");
	m_hTechniquePlanet	=m_pSkyEffect->GetTechniqueByName("simul_planet");
	flareTexture		=m_pSkyEffect->GetParameterByName(NULL,"flareTexture");

	skyTexture1			=m_pSkyEffect->GetParameterByName(NULL,"skyTexture1");
	skyTexture2			=m_pSkyEffect->GetParameterByName(NULL,"skyTexture2");

	m_hTechniqueQuery	=m_pSkyEffect->GetTechniqueByName("simul_query");
	fadeTable->SetCallback(this);
	// CreateSkyTexture() will be called back

	SAFE_RELEASE(flare_texture);
	if(FAILED(hr=D3DXCreateTextureFromFile(m_pd3dDevice,L"Media/Textures/Moon.dds",&flare_texture)))
		return hr;
	SAFE_RELEASE(moon_texture);
	if(FAILED(hr=D3DXCreateTextureFromFile(m_pd3dDevice,L"Media/Textures/Moon.png",&moon_texture)))
		return hr;

	return hr;
}

HRESULT SimulSkyRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	sky_tex_format=D3DFMT_UNKNOWN;
	if(m_pSkyEffect)
        hr=m_pSkyEffect->OnLostDevice();
	SAFE_RELEASE(m_pSkyEffect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(flare_texture);
	SAFE_RELEASE(moon_texture);
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sky_textures[i]);
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(inscatter_textures[i]);
		SAFE_RELEASE(loss_textures_3d[i]);
		SAFE_RELEASE(inscatter_textures_3d[i]);
	}
	SAFE_RELEASE(d3dQuery);
	fadeTable->SetCallback(NULL);
	return hr;
}

HRESULT SimulSkyRenderer::Destroy()
{
	HRESULT hr=S_OK;
	InvalidateDeviceObjects();
	fadeTable->SetCallback(NULL);
	return hr;
}

SimulSkyRenderer::~SimulSkyRenderer()
{
	Destroy();
}

void SimulSkyRenderer::FillSkyTexture(int alt_index,int texture_index,int texel_index,int num_texels,const float *float4_array)
{
	HRESULT hr;
	LPDIRECT3DTEXTURE9 tex=NULL;
	tex=sky_textures[texture_index];
	if(!tex)
		return;
	texel_index+=alt_index*skyTexSize;
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=tex->LockRect(0,&lockedRect,NULL,NULL)))
		return;
	if(sky_tex_format==D3DFMT_A16B16G16R16F)
	{
		// Convert the array of floats into float16 values for the texture.
		short *short_ptr=(short *)(lockedRect.pBits);
		short_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*float4_array++);
	}
	else
	{
		// Convert the array of floats into float16 values for the texture.
		float *float_ptr=(float *)(lockedRect.pBits);
		float_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*float_ptr++=(*float4_array++);
	}
	hr=tex->UnlockRect(0);
}

void SimulSkyRenderer::GetLossAndInscatterTextures(LPDIRECT3DBASETEXTURE9 *l1,LPDIRECT3DBASETEXTURE9 *l2,
		LPDIRECT3DBASETEXTURE9 *i1,LPDIRECT3DBASETEXTURE9 *i2)
{
	if(numAltitudes<=1)
	{
		*l1=loss_textures[0];
		*l2=loss_textures[1];
		*i1=inscatter_textures[0];
		*i2=inscatter_textures[1];
	}
	else
	{
		*l1=loss_textures_3d[0];
		*l2=loss_textures_3d[1];
		*i1=inscatter_textures_3d[0];
		*i2=inscatter_textures_3d[1];
	}
}

float SimulSkyRenderer::GetAltitudeTextureCoordinate() const
{
	return fadeTable->GetAltitudeTexCoord();
}

float SimulSkyRenderer::GetFadeInterp() const
{
	return fadeTable->GetInterpolation();
}

void SimulSkyRenderer::SetSkyTextureSize(unsigned size)
{
	skyTexSize=size;
	CreateSkyTexture();
}
void SimulSkyRenderer::SetFadeTextureSize(unsigned width,unsigned height,unsigned num_alts)
{
	if(fadeTexWidth==width&&fadeTexHeight==height&&numAltitudes==num_alts)
		return;
	fadeTexWidth=width;
	fadeTexHeight=height;
	numAltitudes=num_alts;
	CreateFadeTextures();
}

void SimulSkyRenderer::CreateFadeTextures()
{
	HRESULT hr;
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(loss_textures[i]);
		SAFE_RELEASE(inscatter_textures[i]);
		SAFE_RELEASE(loss_textures_3d[i]);
		SAFE_RELEASE(inscatter_textures_3d[i]);
	}
	for(int i=0;i<3;i++)
	{
		if(numAltitudes<=1)
		{
			hr=D3DXCreateTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,1,0,sky_tex_format,d3d_memory_pool,&loss_textures[i]);
			hr=D3DXCreateTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,1,0,sky_tex_format,d3d_memory_pool,&inscatter_textures[i]);
		}
		else
		{
			hr=D3DXCreateVolumeTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,numAltitudes,1,0,sky_tex_format,d3d_memory_pool,&loss_textures_3d[i]);
			hr=D3DXCreateVolumeTexture(m_pd3dDevice,fadeTexWidth,fadeTexHeight,numAltitudes,1,0,sky_tex_format,d3d_memory_pool,&inscatter_textures_3d[i]);
		}
	}
}

void SimulSkyRenderer::FillFadeTextures(int alt_index,int texture_index,int texel_index,int num_texels,
						const float *loss_float4_array,
						const float *inscatter_float4_array)
{
	HRESULT hr=S_OK;
	D3DLOCKED_RECT lockedRect={0};
	D3DLOCKED_BOX lockedBox={0};
	D3DSURFACE_DESC desc;
	D3DVOLUME_DESC desc3d;
	LPDIRECT3DTEXTURE9 tex=NULL;
	LPDIRECT3DVOLUMETEXTURE9 tex3d=NULL;
	void *tex_ptr=NULL;
	if(numAltitudes<=1)
	{
		tex=loss_textures[texture_index];
		if(!tex)
			return;
		if(FAILED(hr=tex->LockRect(0,&lockedRect,NULL,NULL)))
			return;
		tex->GetLevelDesc(0,&desc);
		tex_ptr=lockedRect.pBits;
	}
	else
	{
		tex3d=loss_textures_3d[texture_index];
		tex3d->GetLevelDesc(0,&desc3d);
		if(!tex3d)
			return;
		if(FAILED(hr=tex3d->LockBox(0,&lockedBox,NULL,NULL)))
			return;
		tex_ptr=lockedBox.pBits;
		texel_index+=desc3d.Width*desc3d.Height*alt_index;
	}
	// Convert the array of floats into float16 values for the texture.
	if(sky_tex_format==D3DFMT_A16B16G16R16F)
	{
		short *short_ptr=(short *)(tex_ptr);
		short_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*loss_float4_array++);
	}
	else
	{
		// Copy the array of floats into the texture.
		float *float_ptr=(float *)(tex_ptr);
		float_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*float_ptr++=(*loss_float4_array++);
	}
	if(numAltitudes<=1)
	{
		hr=tex->UnlockRect(0);
		tex=inscatter_textures[texture_index];
		if(!tex)
			return;
		if(FAILED(hr=tex->LockRect(0,&lockedRect,NULL,NULL)))
			return;
		tex->GetLevelDesc(0,&desc);
		tex_ptr=lockedRect.pBits;
	}
	else
	{
		hr=tex3d->UnlockBox(0);
		tex3d=inscatter_textures_3d[texture_index];
		tex3d->GetLevelDesc(0,&desc3d);
		if(!tex3d)
			return;
		if(FAILED(hr=tex3d->LockBox(0,&lockedBox,NULL,NULL)))
			return;
		tex_ptr=lockedBox.pBits;
		// already added:
		//texel_index+=desc3d.Width*desc3d.Height*alt_index;
	}
	// Convert the array of floats into float16 values for the texture.
	if(sky_tex_format==D3DFMT_A16B16G16R16F)
	{
		short *short_ptr=(short *)(tex_ptr);
		short_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*short_ptr++=simul::sky::TextureGenerator::ToFloat16(*inscatter_float4_array++);
	}
	else
	{
		// Convert the array of floats into float16 values for the texture.
		float *float_ptr=(float *)(tex_ptr);
		float_ptr+=4*texel_index;
		for(int i=0;i<num_texels*4;i++)
			*float_ptr++=(*inscatter_float4_array++);
	}
	if(numAltitudes<=1)
		hr=tex->UnlockRect(0);
	else
		hr=tex3d->UnlockBox(0);

}

void SimulSkyRenderer::CycleTexturesForward()
{
	std::swap(sky_textures[0],sky_textures[1]);
	std::swap(sky_textures[1],sky_textures[2]);
	std::swap(loss_textures[0],loss_textures[1]);
	std::swap(loss_textures[1],loss_textures[2]);
	std::swap(inscatter_textures[0],inscatter_textures[1]);
	std::swap(inscatter_textures[1],inscatter_textures[2]);
	
	std::swap(loss_textures_3d[0],loss_textures_3d[1]);
	std::swap(loss_textures_3d[1],loss_textures_3d[2]);
	std::swap(inscatter_textures_3d[0],inscatter_textures_3d[1]);
	std::swap(inscatter_textures_3d[1],inscatter_textures_3d[2]);
}

HRESULT SimulSkyRenderer::CreateSkyTexture()
{
	HRESULT hr=S_OK;
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(sky_textures[i]);
	}
	for(int i=0;i<3;i++)
	{
		if(numAltitudes<=1)
		{
			if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,skyTexSize,1,1,0,sky_tex_format,d3d_memory_pool,&sky_textures[i])))
				return hr;
		}
		else
		{
			if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,skyTexSize,numAltitudes,1,0,sky_tex_format,d3d_memory_pool,&sky_textures[i])))
				return hr;
		}
	}
	return hr;
}

HRESULT SimulSkyRenderer::CreateSkyEffect()
{
	HRESULT hr=CreateDX9Effect(m_pd3dDevice,m_pSkyEffect,"simul_sky.fx",1,"USE_ALTITUDE_INTERPOLATION");
	return hr;
}

struct Vertex_t
{
	float x,y,z;
};
	static const float size=250.f;
	static Vertex_t vertices[36] =
	{
		{-size,		-size,	size},
		{size,		-size,	size},
		{size,		size,	size},
		{size,		size,	size},
		{-size,		size,	size},
		{-size,		-size,	size},
		
		{-size,		-size,	-size},
		{size,		-size,	-size},
		{size,		size,	-size},
		{size,		size,	-size},
		{-size,		size,	-size},
		{-size,		-size,	-size},
		
		{-size,		size,	-size},
		{size,		size,	-size},
		{size,		size,	size},
		{size,		size,	size},
		{-size,		size,	size},
		{-size,		size,	-size},
					
		{-size,		-size,  -size},
		{size,		-size,	-size},
		{size,		-size,	size},
		{size,		-size,	size},
		{-size,		-size,	size},
		{-size,		-size,  -size},
		
		{size,		-size,	-size},
		{size,		size,	-size},
		{size,		size,	size},
		{size,		size,	size},
		{size,		-size,	size},
		{size,		-size,	-size},
					
		{-size,		-size,	-size},
		{-size,		size,	-size},
		{-size,		size,	size},
		{-size,		size,	size},
		{-size,		-size,	size},
		{-size,		-size,	-size},
	};

HRESULT SimulSkyRenderer::RenderAngledQuad(D3DXVECTOR4 dir,float half_angle_radians)
{
	float Yaw=atan2(dir.x,dir.z);
	float Pitch=-asin(dir.y);
	HRESULT hr=S_OK;
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixIdentity(&world);
	D3DXMatrixRotationYawPitchRoll(
		  &world,
		  Yaw,
		  Pitch,
		  0
		);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));
	struct Vertext
	{
		float x,y,z;
		float tx,ty;
	};
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	static float w=1.f;
	float d=w/tan(half_angle_radians);
	Vertext vertices[4] =
	{
		{-w,-w,	d, 0.f	,0.f},
		{ w,-w,	d, 1.f	,0.f},
		{ w, w,	d, 1.f	,1.f},
		{-w, w,	d, 0.f	,1.f},
	};
	m_pd3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX0);
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr=m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
     
	UINT passes=1;
	hr=m_pSkyEffect->Begin(&passes,0);
	for(unsigned i=0;i<passes;++i)
	{
		hr=m_pSkyEffect->BeginPass(i);
		m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLEFAN,2,vertices,sizeof(Vertext));
		hr=m_pSkyEffect->EndPass();
	}
	hr=m_pSkyEffect->End();
	return hr;
}
int query_issued=0;
void SimulSkyRenderer::CalcSunOcclusion(float cloud_occlusion)
{
	if(!m_hTechniqueQuery)
		return;
	PIXBeginNamedEvent(0,"Sun Occlusion Query");
	m_pSkyEffect->SetTechnique(m_hTechniqueQuery);
	D3DXVECTOR4 sun_dir(skyNode->GetDirectionToSun());
	std::swap(sun_dir.y,sun_dir.z);
	float sun_angular_size=3.14159f/180.f/2.f;

	// fix the projection matrix so this quad is far away:
	D3DXMATRIX tmp=proj;
	float zNear=-proj._43/proj._33;
	static float ff=0.0001f;
	float zFar=(1.f+ff)/tan(sun_angular_size);
	zNear=zFar*ff;
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);
	HRESULT hr;
	// Start the query
	if(!query_issued)
	{
		hr=d3dQuery->Issue(D3DISSUE_BEGIN);
		hr=RenderAngledQuad(sun_dir,sun_angular_size);
		query_issued=1;
		// End the query, get the data
		if(query_issued==1)
    		hr=d3dQuery->Issue(D3DISSUE_END);
	}
	else
	{
		query_issued=2;
    	// Loop until the data becomes available
    	DWORD pixelsVisible=0;
		if(d3dQuery->GetData((void *)&pixelsVisible,sizeof(DWORD),0)!=S_FALSE)//D3DGETDATA_FLUSH
		{
			sun_occlusion=1.f-(float)pixelsVisible/500.f;
			if(sun_occlusion<0)
				sun_occlusion=0;
			sun_occlusion=1.f-(1.f-cloud_occlusion)*(1.f-sun_occlusion);
			query_issued=0;
		}
	}
	proj=tmp;
	PIXEndNamedEvent();
}

HRESULT SimulSkyRenderer::RenderSun()
{
	float alt_km=0.001f*cam_pos.y;
	sunlight=skyInterface->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=2700.f;
	m_pSkyEffect->SetVector(colour	,(D3DXVECTOR4*)(&sunlight));
	m_pSkyEffect->SetTechnique(m_hTechniqueSun);
	D3DXVECTOR4 sun_dir(skyNode->GetDirectionToSun());
	std::swap(sun_dir.y,sun_dir.z);
	float sun_angular_size=3.14159f/180.f/2.f;
	HRESULT hr=RenderAngledQuad(sun_dir,sun_angular_size);
	return hr;
}

HRESULT SimulSkyRenderer::RenderMoon()
{
	float alt_km=0.001f*cam_pos.y;
	m_pSkyEffect->SetTechnique(m_hTechniquePlanet);
	m_pSkyEffect->SetTexture(flareTexture,moon_texture);
	D3DXVECTOR4 sun_dir(skyNode->GetDirectionToSun());
	std::swap(sun_dir.y,sun_dir.z);
	m_pSkyEffect->SetVector	(lightDirection	,&sun_dir);
	float moon_elevation=15.f*3.14159f/180.f;
	simul::sky::float4 original_irradiance=skyInterface->GetSunIrradiance();
	// The moon has an albedo of 0.12:
	simul::sky::float4 moon_colour=0.12f*original_irradiance*skyInterface->GetIsotropicColourLossFactor(alt_km,moon_elevation,1e10f);
	m_pSkyEffect->SetVector(colour,(D3DXVECTOR4*)(&moon_colour));
	D3DXVECTOR4 moon_dir(cos(moon_elevation),sin(moon_elevation),0,0);
	// Make it 5 times bigger than it should be:
	float moon_angular_size=5*   3.14159f/180.f/2.f;
	// Start the query
	HRESULT hr=RenderAngledQuad(moon_dir,moon_angular_size);
	return hr;
}

HRESULT SimulSkyRenderer::RenderFlare(float exposure)
{
	HRESULT hr=S_OK;
	if(!m_pSkyEffect)
		return hr;
	float magnitude=exposure*(1.f-sun_occlusion);
	if(magnitude>1.f)
		magnitude=1.f;
	float alt_km=0.001f*cam_pos.y;
	sunlight=skyInterface->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.5f;
	sunlight*=sun_mult*magnitude;
    m_pd3dDevice->SetVertexShader( NULL );
    m_pd3dDevice->SetPixelShader( NULL );
	m_pSkyEffect->SetVector(colour	,(D3DXVECTOR4*)(&sunlight));
	m_pSkyEffect->SetTechnique(m_hTechniqueFlare);
	m_pSkyEffect->SetTexture(flareTexture,flare_texture);
	float sun_angular_size=3.14159f/180.f/2.f;
	D3DXVECTOR4 sun_dir(skyNode->GetDirectionToSun());
	std::swap(sun_dir.y,sun_dir.z);
	if(magnitude>0.f)
		hr=RenderAngledQuad(sun_dir,sun_angular_size*20.f);
	return hr;
}

HRESULT SimulSkyRenderer::Render()
{
	HRESULT hr=S_OK;

	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	
	hr=RenderSun();
	hr=RenderMoon();

	D3DXMatrixIdentity(&world);
	//set up matrices
	world._41=cam_pos.x;
	world._42=cam_pos.y;
	world._43=cam_pos.z;
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	m_pSkyEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&tmp1));

	//m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  // hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
	//m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
   // m_pd3dDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);


	PIXBeginNamedEvent(0,"Render Sky");
	m_pSkyEffect->SetTexture(skyTexture1, sky_textures[0]);
	m_pSkyEffect->SetTexture(skyTexture2, sky_textures[1]);

	hr=m_pd3dDevice->SetVertexDeclaration( m_pVtxDecl );
	m_pSkyEffect->SetTechnique( m_hTechniqueSky );

	simul::sky::float4 mie_rayleigh_ratio=skyInterface->GetMieRayleighRatio();
	D3DXVECTOR4 ratio(mie_rayleigh_ratio);
	D3DXVECTOR4 sun_dir(skyNode->GetDirectionToSun());
	//if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);

	m_pSkyEffect->SetVector	(lightDirection		,&sun_dir);
	m_pSkyEffect->SetVector	(MieRayleighRatio	,&ratio);
	m_pSkyEffect->SetFloat	(hazeEccentricity	,skyNode->GetMieEccentricity());
	m_pSkyEffect->SetFloat	(altitudeTexCoord	,GetAltitudeTextureCoordinate());
	m_pSkyEffect->SetFloat	(overcastFactor		,overcast_factor);

	m_pSkyEffect->SetFloat	(skyInterp		,interp);
	UINT passes=1;
	hr=m_pSkyEffect->Begin( &passes, 0 );
	for(unsigned i = 0 ; i < passes ; ++i )
	{
		hr=m_pSkyEffect->BeginPass(i);
		hr=m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,12,vertices,sizeof(Vertex_t));
		hr=m_pSkyEffect->EndPass();
	}
	hr=m_pSkyEffect->End();
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    hr=m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
	D3DXMatrixIdentity(&world);
	PIXEndNamedEvent();
	return hr;
}

void SimulSkyRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void SimulSkyRenderer::Update(float dt)
{
	static bool pause=false;
    if(!pause)
	{
		fadeTable->SetAltitudeKM(cam_pos.y*0.001f);
		skyNode->TimeStep(dt);
		fadeTable->Update();
		interp=fadeTable->GetInterpolation();
	}
}

void SimulSkyRenderer::SetTimeMultiplier(float tm)
{
	skyNode->SetTimeMultiplier(tm);
}