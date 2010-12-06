// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulCloudRendererDX11.cpp A renderer for 3d clouds.

#include "SimulCloudRendererDX11.h"
#ifdef DO_SOUND
#include "Simul/Sound/FMOD/NodeSound.h"
#endif
#include "Simul/Base/Timer.h"
#include <fstream>
#include <math.h>


#include <tchar.h>
#include <dxerr.h>
#include <string>
typedef std::basic_string<TCHAR> tstring;
static tstring filepath=TEXT("");
#define PIXBeginNamedEvent(a,b)		// D3DPERF_BeginEvent(a,L##b)
#define PIXEndNamedEvent()			// D3DPERF_EndEvent()
DXGI_FORMAT illumination_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
const bool big_endian=false;
static unsigned default_mip_levels=0;
static unsigned bits[4]={	(unsigned)0x0F00,
							(unsigned)0x000F,
							(unsigned)0x00F0,
							(unsigned)0xF000};
static unsigned bits8[4]={
							(unsigned)0x000000FF,
							(unsigned)0xFF000000,
							(unsigned)0x00FF0000,
							(unsigned)0x0000FF00
};


float thunder_volume=0.f;
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/ThunderCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/CloudGeometryHelper.h"
#include "Simul/Clouds/CloudKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/LicenseKey.h"
#include "CreateEffectDX11.h"


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#ifndef V_RETURN
#define V_RETURN(x)    { hr = x; if( FAILED(hr) ) { return hr; } }
#endif

#ifndef FAIL_RETURN
#define FAIL_RETURN(x)    { hr = x; if( FAILED(hr) ) { return; } }
#endif


typedef std::basic_string<TCHAR> tstring;


struct float2
{
	float x,y;
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
	}
};
struct float3
{
	float x,y,z;
	void operator=(const float*f)
	{
		x=f[0];
		y=f[1];
		z=f[2];
	}
};
struct CloudVertex_t
{
    float3 position;
    float3 texCoords;
    float layerFade;
    float2 texCoordsNoise;
};
struct PosTexVert_t
{
    float3 position;	
    float2 texCoords;
};
CloudVertex_t *vertices=NULL;
PosTexVert_t *lightning_vertices=NULL;
#define MAX_VERTICES (4000)

SimulCloudRendererDX11::SimulCloudRendererDX11() :
	simul::clouds::BaseCloudRenderer(),
	m_hTechniqueLightning(NULL),
	m_pd3dDevice(NULL),
	m_pImmediateContext(NULL),
	m_pVtxDecl(NULL),
	m_pLightningVtxDecl(NULL),
	m_pCloudEffect(NULL),
	m_pLightningEffect(NULL),
	noise_texture(NULL),
	lightning_texture(NULL),
	illumination_texture(NULL),
	sky_loss_texture_1(NULL),
	sky_loss_texture_2(NULL),
	sky_inscatter_texture_1(NULL),
	sky_inscatter_texture_2(NULL),
	skyLossTexture1Resource(NULL),
	skyLossTexture2Resource(NULL),
	skyInscatterTexture1Resource(NULL),
	skyInscatterTexture2Resource(NULL),
	y_vertical(true),
	enable_lightning(false),
	lightning_active(false),
	last_time(0.f),
	timing(0.f),
	noise_texture_frequency(16),
	texture_octaves(6),
	texture_persistence(0.7f),
	noise_texture_size(512)
	,mapped(-1)
{
	lightning_colour.x=0.75f;
	lightning_colour.y=0.75f;
	lightning_colour.z=1.2f;

	D3DXMatrixIdentity(&world);
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	for(int i=0;i<3;i++)
		cloud_textures[i]=NULL;

	cloudNode->SetLicense(SIMUL_LICENSE_KEY);
	cloudNode->SetCacheNoise(true);
	helper->SetYVertical(y_vertical);

	cam_pos.x=cam_pos.y=cam_pos.z=cam_pos.w=0;
	texel_index[0]=texel_index[1]=texel_index[2]=texel_index[3]=0;

	cloudKeyframer->SetFillTexturesAsBlocks(false);
	cloudKeyframer->SetBits(simul::clouds::CloudKeyframer::BRIGHTNESS,
							simul::clouds::CloudKeyframer::AMBIENT,
							simul::clouds::CloudKeyframer::DENSITY,
							simul::clouds::CloudKeyframer::SECONDARY);
#ifdef DO_SOUND
	sound=new simul::sound::fmod::NodeSound();
	sound->Init("Media/Sound/IntelDemo.fev");
	int ident=sound->GetOrAddSound("rain");
	sound->StartSound(ident,0);
	sound->SetSoundVolume(ident,0.f);
#endif
}

void SimulCloudRendererDX11::SetSkyInterface(simul::sky::BaseSkyInterface *si)
{
	skyInterface=si;
	cloudKeyframer->SetSkyInterface(si);
}

void SimulCloudRendererDX11::SetFadeTableInterface(simul::sky::FadeTableInterface *fti)
{
	fadeTableInterface=fti;
}

void SimulCloudRendererDX11::SetLossTextures(ID3D11Resource* t1,ID3D11Resource* t2)
{
	if(sky_loss_texture_1!=t1)
	{
		sky_loss_texture_1=static_cast<ID3D11Texture2D*>(t1);
		SAFE_RELEASE(skyLossTexture1Resource);
		m_pd3dDevice->CreateShaderResourceView(sky_loss_texture_1,NULL,&skyLossTexture1Resource);
	}
	if(sky_loss_texture_2!=t2)
	{
		sky_loss_texture_2=static_cast<ID3D11Texture2D*>(t2);
		SAFE_RELEASE(skyLossTexture2Resource);
		m_pd3dDevice->CreateShaderResourceView(sky_loss_texture_2,NULL,&skyLossTexture2Resource);
	}
}

void SimulCloudRendererDX11::SetInscatterTextures(ID3D11Resource* t1,ID3D11Resource* t2)
{
	if(sky_inscatter_texture_1!=t1)
	{
		sky_inscatter_texture_1=static_cast<ID3D11Texture2D*>(t1);
		SAFE_RELEASE(skyInscatterTexture1Resource);
		m_pd3dDevice->CreateShaderResourceView(sky_inscatter_texture_1,NULL,&skyInscatterTexture1Resource);
	}
	if(sky_inscatter_texture_1!=t2)
	{
		sky_inscatter_texture_2=static_cast<ID3D11Texture2D*>(t2);
		SAFE_RELEASE(skyInscatterTexture2Resource);
		m_pd3dDevice->CreateShaderResourceView(sky_inscatter_texture_2,NULL,&skyInscatterTexture2Resource);
	}
}

void SimulCloudRendererDX11::SetNoiseTextureProperties(int s,int f,int o,float p)
{
	noise_texture_size=s;
	noise_texture_frequency=f;
	texture_octaves=o;
	texture_persistence=p;
	CreateNoiseTexture();
}

HRESULT SimulCloudRendererDX11::RestoreDeviceObjects( ID3D11Device* dev)
{
	m_pd3dDevice=dev;
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	HRESULT hr;
	//V_RETURN(m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl))
	//V_RETURN(m_pd3dDevice->CreateVertexDeclaration(std_decl,&m_pLightningVtxDecl))
	V_RETURN(CreateNoiseTexture());
	V_RETURN(CreateLightningTexture());
	V_RETURN(CreateIlluminationTexture());
	V_RETURN(CreateCloudEffect());

	m_hTechniqueCloud					=m_pCloudEffect->GetTechniqueByName("simul_clouds");
	m_hTechniqueCloudsAndLightning		=m_pCloudEffect->GetTechniqueByName("simul_clouds_and_lightning");

	worldViewProj						=m_pCloudEffect->GetVariableByName("worldViewProj")->AsMatrix();
	eyePosition							=m_pCloudEffect->GetVariableByName("eyePosition")->AsVector();
	lightResponse						=m_pCloudEffect->GetVariableByName("lightResponse")->AsVector();
	lightDir							=m_pCloudEffect->GetVariableByName("lightDir")->AsVector();
	skylightColour						=m_pCloudEffect->GetVariableByName("skylightColour")->AsVector();
	sunlightColour						=m_pCloudEffect->GetVariableByName("sunlightColour")->AsVector();
	fractalScale						=m_pCloudEffect->GetVariableByName("fractalScale")->AsVector();
	interp								=m_pCloudEffect->GetVariableByName("interp")->AsScalar();
	mieRayleighRatio					=m_pCloudEffect->GetVariableByName("mieRayleighRatio")->AsVector();
	hazeEccentricity					=m_pCloudEffect->GetVariableByName("hazeEccentricity")->AsScalar();
	fadeInterp							=m_pCloudEffect->GetVariableByName("fadeInterp")->AsScalar();
	cloudEccentricity					=m_pCloudEffect->GetVariableByName("cloudEccentricity")->AsScalar();
	altitudeTexCoord					=m_pCloudEffect->GetVariableByName("altitudeTexCoord")->AsScalar();

	//if(enable_lightning)
	{
		lightningMultipliers			=m_pCloudEffect->GetVariableByName("lightningMultipliers")->AsVector();
		lightningColour					=m_pCloudEffect->GetVariableByName("lightningColour")->AsVector();
		illuminationOrigin				=m_pCloudEffect->GetVariableByName("illuminationOrigin")->AsVector();
		illuminationScales				=m_pCloudEffect->GetVariableByName("illuminationScales")->AsVector();
	}

	cloudDensity1						=m_pCloudEffect->GetVariableByName("cloudDensity1")->AsShaderResource();
	cloudDensity2						=m_pCloudEffect->GetVariableByName("cloudDensity2")->AsShaderResource();
	noiseTexture						=m_pCloudEffect->GetVariableByName("noiseTexture")->AsShaderResource();
	lightningIlluminationTexture		=m_pCloudEffect->GetVariableByName("lightningIlluminationTexture")->AsShaderResource();
	skyLossTexture1						=m_pCloudEffect->GetVariableByName("skyLossTexture1")->AsShaderResource();
	skyLossTexture2						=m_pCloudEffect->GetVariableByName("skyLossTexture2")->AsShaderResource();
	skyInscatterTexture1				=m_pCloudEffect->GetVariableByName("skyInscatterTexture1")->AsShaderResource();
	skyInscatterTexture2				=m_pCloudEffect->GetVariableByName("skyInscatterTexture2")->AsShaderResource();
	D3D11_SHADER_RESOURCE_VIEW_DESC texdesc;
	/*{
    DXGI_FORMAT Format;
    D3D11_SRV_DIMENSION ViewDimension;
    union {
        D3D11_BUFFER_SRV Buffer;
        D3D11_TEX1D_SRV Texture1D;
        D3D11_TEX1D_ARRAY_SRV Texture1DArray;
        D3D11_TEX2D_SRV Texture2D;
        D3D11_TEX2D_ARRAY_SRV Texture2DArray;
        D3D11_TEX2DMS_SRV Texture2DMS;
        D3D11_TEX2DMS_ARRAY_SRV Texture2DMSArray;
        D3D11_TEX3D_SRV Texture3D;
        D3D11_TEXCUBE_SRV TextureCube;
    };
	};*/
	texdesc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
	texdesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE3D;
	texdesc.Texture3D.MostDetailedMip=0;
	texdesc.Texture3D.MipLevels=1;
    V_RETURN(m_pd3dDevice->CreateShaderResourceView(noise_texture,NULL,&noiseTextureResource));
    V_RETURN(m_pd3dDevice->CreateShaderResourceView(illumination_texture,NULL,&lightningIlluminationTextureResource ));

	noiseTexture				->SetResource(noiseTextureResource);
	lightningIlluminationTexture->SetResource(lightningIlluminationTextureResource);

	V_RETURN(CreateEffect(m_pd3dDevice,&m_pLightningEffect,L"media\\HLSL\\simul_lightning.fx"));
	if(m_pLightningEffect)
	{
		m_hTechniqueLightning	=m_pLightningEffect->GetTechniqueByName("simul_lightning");
		l_worldViewProj			=m_pLightningEffect->GetVariableByName("worldViewProj")->AsMatrix();
	}

	
	const D3D11_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	1, DXGI_FORMAT_R32_FLOAT,			0,	24,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	2, DXGI_FORMAT_R32G32_FLOAT,		0,	28,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
	const D3D11_INPUT_ELEMENT_DESC std_decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
	D3DX11_PASS_DESC PassDesc;
	ID3DX11EffectPass *pass=m_hTechniqueCloud->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);

	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout( decl,4,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
	// Create the main vertex buffer:
	D3D11_BUFFER_DESC desc=
	{
        MAX_VERTICES*sizeof(CloudVertex_t),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_VERTEX_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
	};
	if(!vertices)
		vertices=new CloudVertex_t[MAX_VERTICES];
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(CloudVertex_t);
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);

	if(m_hTechniqueLightning)
	{
		pass=m_hTechniqueLightning->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);
		hr=m_pd3dDevice->CreateInputLayout( std_decl, 2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pLightningVtxDecl);
	}
	cloudKeyframer->SetRenderCallback(NULL);
	cloudKeyframer->SetRenderCallback(this);
	return hr;
}

HRESULT SimulCloudRendererDX11::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(m_pLightningVtxDecl);
	SAFE_RELEASE(m_pCloudEffect);
	SAFE_RELEASE(m_pLightningEffect);
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		SAFE_RELEASE(cloudDensityResource[i]);
	}
	SAFE_RELEASE(noise_texture);
	SAFE_RELEASE(lightning_texture);
	SAFE_RELEASE(illumination_texture);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(skyLossTexture1Resource);
	SAFE_RELEASE(skyLossTexture2Resource);
	SAFE_RELEASE(skyInscatterTexture1Resource);
	SAFE_RELEASE(skyInscatterTexture2Resource);

	SAFE_RELEASE(noiseTextureResource);
	
	SAFE_RELEASE(lightningIlluminationTextureResource);

	return hr;
}

HRESULT SimulCloudRendererDX11::Destroy()
{
	HRESULT hr=S_OK;
	hr=InvalidateDeviceObjects();
#ifdef DO_SOUND
	delete sound;
	sound=NULL;
#endif
	return hr;
}

SimulCloudRendererDX11::~SimulCloudRendererDX11()
{
	Destroy();
}

bool SimulCloudRendererDX11::CreateNoiseTexture(bool override_file)
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(noise_texture);
	//if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,size,default_mip_levels,default_texture_usage,DXGI_FORMAT_R8G8B8A8_UINT,D3DPOOL_MANAGED,&noise_texture)))
	//	return hr;
	D3D11_TEXTURE2D_DESC desc=
	{
		noise_texture_size,
		noise_texture_size,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		{1,0},
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	hr=m_pd3dDevice->CreateTexture2D(&desc,NULL,&noise_texture);
	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(hr=m_pImmediateContext->Map(noise_texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))
		return false;
	//cloudKeyframer->SetBits(bits8[0],bits8[1],bits8[2],bits8[3],(unsigned)4,big_endian);
	simul::clouds::TextureGenerator::Make2DNoiseTexture((unsigned char *)(mapped.pData),noise_texture_size,noise_texture_frequency,texture_octaves,texture_persistence);
	m_pImmediateContext->Unmap(noise_texture,0);
	//noise_texture->GenerateMipSubLevels();
	return true;
}

HRESULT SimulCloudRendererDX11::CreateLightningTexture()
{
	HRESULT hr=S_OK;
	unsigned size=64;
	SAFE_RELEASE(lightning_texture);
	D3D11_TEXTURE1D_DESC desc=
	{
		size,
		1,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	if(FAILED(hr=m_pd3dDevice->CreateTexture1D(&desc,NULL,&lightning_texture)))
		return hr;
	D3D11_MAPPED_SUBRESOURCE resource;
	if(FAILED(hr=m_pImmediateContext->Map(lightning_texture,0,D3D11_MAP_WRITE_DISCARD,0,&resource)))
		return hr;
	unsigned char *lightning_tex_data=(unsigned char *)(resource.pData);
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float level=.5f*linear*linear+5.f*(linear>.97f);
		float r=lightning_colour.x*level;
		float g=lightning_colour.y*level;
		float b=lightning_colour.z*level;
		if(r>1.f)
			r=1.f;
		if(g>1.f)
			g=1.f;
		if(b>1.f)
			b=1.f;
		lightning_tex_data[4*i+0]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+1]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+2]=(unsigned char)(255.f*g);
		lightning_tex_data[4*i+3]=(unsigned char)(255.f*r);
	}
	m_pImmediateContext->Unmap(lightning_texture,0);
	return hr;
}

HRESULT SimulCloudRendererDX11::CreateIlluminationTexture()
{
	unsigned w=64;
	unsigned l=64;
	unsigned h=8;
	SAFE_RELEASE(illumination_texture);
	D3D11_TEXTURE3D_DESC desc=
	{
		w,l,h,
		1,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	HRESULT hr=m_pd3dDevice->CreateTexture3D(&desc,0,&illumination_texture);
	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(hr=m_pImmediateContext->Map(illumination_texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))
		return hr;
	memset(mapped.pData,0,4*w*l*h);
	m_pImmediateContext->Unmap(illumination_texture,0);
	return hr;
}
float t[4];
float u[4];
HRESULT SimulCloudRendererDX11::UpdateIlluminationTexture(float dt)
{
	HRESULT hr=S_OK;
	// RGBA bit-shift is 12,8,4,0
	simul::clouds::TextureGenerator::SetBits((unsigned)255<<16,(unsigned)255<<8,(unsigned)255<<0,(unsigned)255<<24,4,false);

	unsigned w=64;
	unsigned l=64;
	unsigned h=8;
	simul::math::Vector3 X1,X2,DX;
	DX.Define(lightningRenderInterface->GetLightningZoneSize(),
		lightningRenderInterface->GetLightningZoneSize(),
		cloudNode->GetCloudBaseZ()+cloudNode->GetCloudHeight());
	X1=lightningRenderInterface->GetLightningCentreX()-0.5f*DX;
	X2=lightningRenderInterface->GetLightningCentreX()+0.5f*DX;
	X1.z=0;
	unsigned max_texels=w*l*h;
	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(hr=m_pImmediateContext->Map(illumination_texture,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))
		return hr;
	unsigned char *lightning_cloud_tex_data=(unsigned char *)(mapped.pData);
	// lightning rate : strikes per second
	static float rr=1.7f;
	float lightning_rate=rr;
	lightning_rate*=4.f;
	unsigned texels=(unsigned)(lightning_rate*dt/4.f*(float)max_texels);
	
	for(unsigned i=0;i<1;i++)
	{
		//thunder_volume[i]*=0.95f;
		if(lightningRenderInterface->GetSourceStarted(i))
		{
			u[i]=lightningRenderInterface->GetLightSourceProgress(i);
			if(u[i]<0.5f)
			{
				static float ff=0.05f;
				thunder_volume+=ff;
			}
			continue;
		}
		else
			u[i]=0;
		if(!lightningRenderInterface->CanStartSource(i))
			continue;
		t[i]=(float)texel_index[i]/(float)(max_texels);
		unsigned &index=texel_index[i];
		
		simul::clouds::TextureGenerator::PartialMake3DLightningTexture(
			lightningRenderInterface,i,
			w,l,h,
			X1,X2,
			index,
			texels,
			lightning_cloud_tex_data,
			cloudInterface->GetUseTbb());
		
		if(lightning_active&&index>=max_texels)
		{
			index=0;
			lightningRenderInterface->StartSource(i);
			float x1[3],br1;
			lightningRenderInterface->GetSegmentVertex(0,0,0,br1,x1);
#ifdef DO_SOUND
			char sound_name[20];
			sprintf(sound_name,"thunderclap%d",i);
			int ident=sound->GetOrAddSound(sound_name);
			sound->StartSound(ident,x1);
#endif
		}
	}
	m_pImmediateContext->Unmap(illumination_texture,0);
	return hr;
}
void SimulCloudRendererDX11::Unmap()
{
	if(mapped!=-1)
	{
		if(mapped>=0)
		{
			ID3D11Texture3D  *mapped_dx11_resource=(ID3D11Texture3D*)cloud_textures[mapped];
			m_pImmediateContext->Unmap(mapped_dx11_resource,0);
		}
		mapped=-1;
	}
}

void SimulCloudRendererDX11::Map(int texture_index)
{
	HRESULT hr=S_OK;
	if(mapped!=texture_index)
	{
		Unmap();
		ID3D11Texture3D *dx11_resource=cloud_textures[texture_index];
		hr=m_pImmediateContext->Map(dx11_resource,0,D3D11_MAP_WRITE_DISCARD,0,&mapped_cloud_texture);
		mapped=texture_index;
	}
}
// implementing CloudRenderCallback:
static int mapped=-1;
void SimulCloudRendererDX11::SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z)
{
	if(!width_x||!length_y||!depth_z)
		return;
	if(width_x==cloud_tex_width_x&&length_y==cloud_tex_length_y&&depth_z==cloud_tex_depth_z)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=depth_z;
	HRESULT hr=S_OK;
	static DXGI_FORMAT cloud_tex_format=DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D11_TEXTURE3D_DESC desc=
	{
		width_x,length_y,depth_z,
		1,
		cloud_tex_format,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0
	};
	for(int i=0;i<3;i++)
	{
		SAFE_RELEASE(cloud_textures[i]);
		if(FAILED(hr=m_pd3dDevice->CreateTexture3D(&desc,0,&cloud_textures[i])))
			return;
	}
    FAIL_RETURN(m_pd3dDevice->CreateShaderResourceView(cloud_textures[0],NULL,&cloudDensityResource[0]));
    FAIL_RETURN(m_pd3dDevice->CreateShaderResourceView(cloud_textures[1],NULL,&cloudDensityResource[1]));
    FAIL_RETURN(m_pd3dDevice->CreateShaderResourceView(cloud_textures[2],NULL,&cloudDensityResource[2]));
	hr=m_pImmediateContext->Map(cloud_textures[2],0,D3D11_MAP_WRITE_DISCARD,0,&mapped_cloud_texture);
	mapped=2;
}

void SimulCloudRendererDX11::FillCloudTextureSequentially(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array)
{
	Map(texture_index);
	unsigned *ptr=(unsigned *)mapped_cloud_texture.pData;
	ptr+=texel_index;
	memcpy(ptr,uint32_array,num_texels*sizeof(unsigned));
}

void SimulCloudRendererDX11::CycleTexturesForward()
{
	Unmap();
	std::swap(cloud_textures[0],cloud_textures[1]);
	std::swap(cloud_textures[1],cloud_textures[2]);
	std::swap(cloudDensityResource[0],cloudDensityResource[1]);
	std::swap(cloudDensityResource[1],cloudDensityResource[2]);
	Map(2);
}

HRESULT SimulCloudRendererDX11::CreateCloudEffect()
{
	return CreateEffect(m_pd3dDevice,&m_pCloudEffect,L"media\\HLSL\\simul_clouds_and_lightning.fx");
}

void SimulCloudRendererDX11::Update(float dt)
{
	if(!cloudInterface)
		return;
	float current_time=skyInterface->GetDaytime();
	float real_dt=0.f;
	if(last_time!=0.f)
		real_dt=3600.f*(current_time-last_time);
	last_time=current_time;

	simul::base::Timer t;
	t.StartTime();
	cloudKeyframer->Update(current_time);
	t.FinishTime();
	timing*=.99f;
	timing+=0.01f*t.Time;
	simul::graph::meta::TimeStepVisitor tsv;
	tsv.SetTimeStep(dt);
	cloudNode->Accept(tsv);
	if(enable_lightning)
		UpdateIlluminationTexture(dt);
}

void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj)
{
	//set up matrices
	D3DXMATRIX tmp1, tmp2;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXMatrixMultiply(&tmp1, &world,&view);
	D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
	D3DXMatrixTranspose(wvp,&tmp2);
}

D3DXVECTOR4 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}


HRESULT SimulCloudRendererDX11::Render(bool)
{
	HRESULT hr=S_OK;
	PIXBeginNamedEvent(1,"Render Clouds Layers");
//	static D3DTEXTUREADDRESS wrap_u=D3DTADDRESS_WRAP,wrap_v=D3DTADDRESS_WRAP,wrap_w=D3DTADDRESS_CLAMP;

	cloudDensity1->SetResource(cloudDensityResource[0]);
	cloudDensity2->SetResource(cloudDensityResource[1]);
	noiseTexture->SetResource(noiseTextureResource);
	skyLossTexture1->SetResource(skyLossTexture1Resource);
	skyInscatterTexture1->SetResource(skyInscatterTexture1Resource);
	skyLossTexture2->SetResource(skyLossTexture2Resource);
	skyInscatterTexture2->SetResource(skyInscatterTexture2Resource);

	// Mess with the proj matrix to extend the far clipping plane:
	// According to the documentation for D3DXMatrixPerspectiveFovLH:
	// proj._33=zf/(zf-zn)  = 1/(1-zn/zf)
	// proj._43=-zn*zf/(zf-zn)
	// so proj._43/proj._33=-zn.

	float zNear=-proj._43/proj._33;
	float zFar=helper->GetMaxCloudDistance()*2.1f;
	proj._33=zFar/(zFar-zNear);
	proj._43=-zNear*zFar/(zFar-zNear);
		
	//set up matrices
	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	cam_pos=GetCameraPosVector(view);
	simul::math::Vector3 X(cam_pos.x,cam_pos.y,cam_pos.z);
	simul::math::Vector3 wind_offset=cloudInterface->GetWindOffset();
	if(y_vertical)
		std::swap(wind_offset.y,wind_offset.z);
	X+=wind_offset;

	worldViewProj->AsMatrix()->SetMatrix(&wvp._11);

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	simul::sky::float4 view_km=(const float*)cam_pos;
	helper->Update((const float*)cam_pos,wind_offset,view_dir,up);
	view_km*=0.001f;
	float alt_km=0.001f*(cloudInterface->GetCloudBaseZ()+.5f*cloudInterface->GetCloudHeight());

	static float ff=0.03f;
	simul::sky::float4 light_response(	cloudInterface->GetLightResponse(),
										ff*cloudInterface->GetSecondaryLightResponse(),
										0,
										0);
	simul::sky::float4 sun_dir=skyInterface->GetDirectionToLight();


	// calculate sun occlusion for any external classes that need it:
	if(y_vertical)
		std::swap(X.y,X.z);
	float len=cloudInterface->GetOpticalPathLength(X.FloatPointer(0),(const float*)sun_dir);
	float vis=min(1.f,max(0.f,exp(-0.001f*cloudInterface->GetOpticalDensity()*len)));
	sun_occlusion=1.f-vis;
	if(y_vertical)
		std::swap(X.y,X.z);

	if(y_vertical)
		std::swap(sun_dir.y,sun_dir.z);
	simul::sky::float4 sky_light_colour=fadeTableInterface->GetAmbientLight(alt_km);
	simul::sky::float4 sunlight=skyInterface->GetLocalIrradiance(alt_km);
	simul::sky::float4 fractal_scales=helper->GetFractalScales(cloudInterface);

	float tan_half_fov_vertical=1.f/proj._22;
	float tan_half_fov_horizontal=1.f/proj._11;
	helper->SetFrustum(tan_half_fov_horizontal,tan_half_fov_vertical);
	
	helper->MakeGeometry(cloudInterface,enable_lightning);

	ID3DX11EffectConstantBuffer* cbUser=m_pCloudEffect->GetConstantBufferByName("cbUser");
	if(cbUser)
	{
		ID3DX11EffectVectorVariable *lr=cbUser->GetMemberByName("lightResponse")->AsVector();
		//if(lr)
		//	lr->SetFloatVector(light_response);
	}

	float cloud_interp=cloudKeyframer->GetInterpolation();
	interp				->AsScalar()->SetFloat			(cloud_interp);
	eyePosition			->AsVector()->SetFloatVector	(cam_pos);
	lightResponse		->AsVector()->SetFloatVector	(light_response);
	lightDir			->AsVector()->SetFloatVector	(sun_dir);
	skylightColour		->AsVector()->SetFloatVector	(sky_light_colour);
	sunlightColour		->AsVector()->SetFloatVector	(sunlight);
	fractalScale		->AsVector()->SetFloatVector	(fractal_scales);
	mieRayleighRatio	->AsVector()->SetFloatVector	(skyInterface->GetMieRayleighRatio());
	cloudEccentricity	->SetFloat						(cloudInterface->GetMieAsymmetry());
	hazeEccentricity	->AsScalar()->SetFloat			(skyInterface->GetMieEccentricity());
	fadeInterp			->AsScalar()->SetFloat			(fade_interp);
	altitudeTexCoord	->AsScalar()->SetFloat			(altitude_tex_coord);

	if(enable_lightning)
	{
		static float bb=.1f;
		simul::sky::float4 lightning_multipliers;
		for(unsigned i=0;i<4;i++)
		{
			if(i<lightningRenderInterface->GetNumLightSources())
				lightning_multipliers[i]=bb*lightningRenderInterface->GetLightSourceBrightness(i);
			else lightning_multipliers[i]=0;
		}
		static float effect_on_cloud=20.f;
		//static float lm=2.f;
		//simul::sky::float4 lightning_colour(lm*lightning_red,lm*lightning_green,lm*lightning_blue,25.f);
		lightning_colour.w=effect_on_cloud;
		lightningMultipliers->SetFloatVector	(lightning_multipliers);
		lightningColour->SetFloatVector	(lightning_colour);

		simul::math::Vector3 light_X1,light_X2,light_DX;
		light_DX.Define(lightningRenderInterface->GetLightningZoneSize(),
			lightningRenderInterface->GetLightningZoneSize(),
			cloudNode->GetCloudBaseZ()+cloudNode->GetCloudHeight());

		light_X1=lightningRenderInterface->GetLightningCentreX();
		light_X1-=0.5f*light_DX;
		light_X1.z=0;
		light_X2=lightningRenderInterface->GetLightningCentreX();
		light_X2+=0.5f*light_DX;
		light_X2.z=light_DX.z;

		illuminationOrigin->SetFloatVector	(light_X1);
		illuminationScales->SetFloatVector	(light_DX);
	}
	int startv=0;
	int v=0;

	//m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	//m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	//m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	// blending for alpha:
	//m_pd3dDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
	//m_pd3dDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA);

	startv=v;
	simul::math::Vector3 pos;
	D3D11_MAPPED_SUBRESOURCE mapped_vertices;
	m_pImmediateContext->Map(vertexBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped_vertices);
	vertices=(CloudVertex_t*)mapped_vertices.pData;
	for(std::vector<simul::clouds::CloudGeometryHelper::RealtimeSlice*>::const_iterator i=helper->GetSlices().begin();
		i!=helper->GetSlices().end();i++)
	{
		helper->MakeLayerGeometry(cloudInterface,*i);
		const std::vector<int> &quad_strip_vertices=helper->GetQuadStripIndices();
		size_t qs_vert=0;
		float fade=(*i)->fadeIn;
		bool start=true;
		for(std::vector<const simul::clouds::CloudGeometryHelper::QuadStrip*>::const_iterator j=(*i)->quad_strips.begin();
			j!=(*i)->quad_strips.end();j++)
		{
			bool l=0;
			for(size_t k=0;k<(*j)->num_vertices;k++,qs_vert++,l++,v++)
			{
				const simul::clouds::CloudGeometryHelper::Vertex &V=helper->GetVertices()[quad_strip_vertices[qs_vert]];
				
				pos.Define(V.x,V.y,V.z);
				simul::math::Vector3 tex_pos(V.cloud_tex_x,V.cloud_tex_y,V.cloud_tex_z);
				if(v>=MAX_VERTICES)
					break;
				if(start)
				{
					CloudVertex_t &startvertex=vertices[v];
					startvertex.position=pos;
				startvertex.texCoords=tex_pos;
				startvertex.texCoordsNoise.x=V.noise_tex_x;
				startvertex.texCoordsNoise.y=V.noise_tex_y;
				startvertex.layerFade=fade;
					v++;
					start=false;
				}
				CloudVertex_t &vertex=vertices[v];
				vertex.position=pos;
				vertex.texCoords=tex_pos;
				vertex.texCoordsNoise.x=V.noise_tex_x;
				vertex.texCoordsNoise.y=V.noise_tex_y;
				vertex.layerFade=fade;
			}
		}
		if(v>=MAX_VERTICES)
			break;
		CloudVertex_t &vertex=vertices[v];
		vertex.position=pos;
		v++;
	}
	m_pImmediateContext->Unmap(vertexBuffer,0);
	UINT stride = sizeof( CloudVertex_t );
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,				// the first input slot for binding
										1,				// the number of buffers in the array
										&vertexBuffer,	// the array of vertex buffers
										&stride,		// array of stride values, one for each buffer
										&offset);		// array of offset values, one for each buffer
	if(enable_lightning)
		m_hTechniqueCloudsAndLightning->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	else
		m_hTechniqueCloud->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	// Set the input layout
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	if((v-startv)>2)
		m_pImmediateContext->Draw((v-startv)-2,0);//D3DPT_TRIANGLESTRIP,(v-startv)-2,&(vertices[startv]),sizeof(CloudVertex_t));
	//hr=m_pCloudEffect->EndPass();
	//hr=m_pCloudEffect->End();

//	m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	PIXEndNamedEvent();
	skyLossTexture1->SetResource(0);
	skyInscatterTexture1->SetResource(0);
	skyLossTexture2->SetResource(0);
	skyInscatterTexture2->SetResource(0);
	return hr;
}

HRESULT SimulCloudRendererDX11::RenderLightning()
{
	if(!enable_lightning)
		return S_OK;
	using namespace simul::clouds;

	if(!lightning_vertices)
		lightning_vertices=new PosTexVert_t[4500];

	HRESULT hr=S_OK;

	PIXBeginNamedEvent(0, "Render Lightning");
//	m_pd3dDevice->SetTexture(0,lightning_texture);
	/*m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);*/

	//m_pCloudEffect->SetTechnique( m_hTechniqueLightning );
		
	//set up matrices
	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	cam_pos=GetCameraPosVector(view);

//    m_pd3dDevice->SetVertexShader( NULL );
   // m_pd3dDevice->SetPixelShader( NULL );

#ifndef XBOX
	//m_pd3dDevice->SetTransform(D3DTS_VIEW,&view);
	//m_pd3dDevice->SetTransform(D3DTS_WORLD,&world);
	//m_pd3dDevice->SetTransform(D3DTS_PROJECTION,&proj);
#endif

	simul::math::Vector3 view_dir	(view._13,view._23,view._33);
	simul::math::Vector3 up			(view._12,view._22,view._32);

	m_pImmediateContext->IASetInputLayout( m_pLightningVtxDecl );

	//m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	//m_pd3dDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	//m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	simul::math::Vector3 pos;

	static float lm=10.f;
	static float main_bright=1.f;
	int vert_start=0;
	int vert_num=0;
	m_hTechniqueLightning->GetPassByIndex(0)->Apply(0,m_pImmediateContext);
	//m_pLightningEffect->SetTechnique( m_hTechniqueLightning );

	l_worldViewProj->SetMatrix(&wvp._11);
//	UINT passes=1;
//	hr=m_pLightningEffect->Begin(&passes,0);
//	hr=m_pLightningEffect->BeginPass(0);
	for(unsigned i=0;i<lightningRenderInterface->GetNumLightSources();i++)
	{
		if(!lightningRenderInterface->GetSourceStarted(i))
			continue;
		simul::math::Vector3 x1,x2;
		float bright1=0.f,bright2=0.f;
		simul::math::Vector3 camPos(cam_pos);
		lightningRenderInterface->GetSegmentVertex(i,0,0,bright1,x1.FloatPointer(0));
		float dist=(x1-camPos).Magnitude();
		float vertical_shift=helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(unsigned jj=0;jj<lightningRenderInterface->GetNumBranches(i);jj++)
		{
			simul::math::Vector3 last_transverse;
			vert_start=vert_num;
			for(unsigned k=0;k<lightningRenderInterface->GetNumSegments(i,jj)&&vert_num<4500;k++)
			{
				lightningRenderInterface->GetSegmentVertex(i,jj,k,bright1,x1.FloatPointer(0));

				static float ww=700.f;
				float width=ww*lightningRenderInterface->GetBranchWidth(i,jj);
				float width1=bright1*width;
				simul::math::Vector3 dx=x2-x1;
				simul::math::Vector3 transverse;
				CrossProduct(transverse,view_dir,dx);
				transverse.Unit();
				transverse*=width1;
				simul::math::Vector3 t=transverse;
				if(k)
					t=.5f*(last_transverse+transverse);
				simul::math::Vector3 x1a=x1-t;
				simul::math::Vector3 x1b=x1+t;
				if(!k)
					bright1=0;
				if(k==lightningRenderInterface->GetNumSegments(i,jj)-1)
					bright2=0;
				PosTexVert_t &v1=lightning_vertices[vert_num++];
				if(y_vertical)
				{
					std::swap(x1a.y,x1a.z);
					std::swap(x1b.y,x1b.z);
				}
				v1.position.x=x1a.x;
				v1.position.y=x1a.y+vertical_shift;
				v1.position.z=x1a.z;
				v1.texCoords.x=0;
				//glMultiTexCoord4f(GL_TEXTURE1,lm*lightning_red,lm*lightning_green,lm*lightning_blue,bright);  //Fade off the tips of end branches.
				PosTexVert_t &v2=lightning_vertices[vert_num++];
				v2.position.x=x1b.x;
				v2.position.y=x1b.y+vertical_shift;
				v2.position.z=x1b.z;
				v2.texCoords.x=1.f;
				//glMultiTexCoord4f(GL_TEXTURE1,lm*lightning_red,lm*lightning_green,lm*lightning_blue,bright);  //Fade off the tips of end branches.
				last_transverse=transverse;
			}
			if(vert_num-vert_start>2)
				m_pImmediateContext->Draw(vert_num-vert_start-2,0);//PrimitiveUP(D3DPT_TRIANGLESTRIP,vert_num-vert_start-2,&(lightning_vertices[vert_start]),sizeof(PosTexVert_t));
		}
	}
//	hr=m_pLightningEffect->EndPass();
//	hr=m_pLightningEffect->End();
	PIXEndNamedEvent();
	return hr;
}

void SimulCloudRendererDX11::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}


simul::clouds::LightningRenderInterface *SimulCloudRendererDX11::GetLightningRenderInterface()
{
	return lightningRenderInterface;
}

float SimulCloudRendererDX11::GetSunOcclusion() const
{
	return sun_occlusion;
}

HRESULT SimulCloudRendererDX11::MakeCubemap()
{
	HRESULT hr=S_OK;
	return hr;
}

const char *SimulCloudRendererDX11::GetDebugText() const
{
	static char debug_text[256];
	simul::math::Vector3 wo=cloudInterface->GetWindOffset();
	sprintf_s(debug_text,256,"\n%2.2g %2.2g %2.2g %2.2g\n%2.2g %2.2g %2.2g %2.2g",t[0],t[1],t[2],t[3],u[0],u[1],u[2],u[3]);
	return debug_text;
}


void SimulCloudRendererDX11::SetEnableStorms(bool s)
{
	enable_lightning=s;
}

float SimulCloudRendererDX11::GetTiming() const
{
	return timing;
}

ID3D11Texture3D* *SimulCloudRendererDX11::GetCloudTextures()
{
	return cloud_textures;
}

const float *SimulCloudRendererDX11::GetCloudScales() const
{
	static float s[3];
	s[0]=1.f/cloudInterface->GetCloudWidth();
	s[1]=1.f/cloudInterface->GetCloudLength();
	s[2]=1.f/cloudInterface->GetCloudHeight();
	return s;
}

const float *SimulCloudRendererDX11::GetCloudOffset() const
{
	static simul::math::Vector3 wind_offset;
	wind_offset=cloudInterface->GetWindOffset();
	return wind_offset.FloatPointer(0);
} 
