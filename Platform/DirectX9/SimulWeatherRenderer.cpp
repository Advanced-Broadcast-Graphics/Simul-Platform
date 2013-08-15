// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRenderer.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
#endif

#include "Simul/Platform/DirectX9/CreateDX9Effect.h"
#include "Simul/Math/Decay.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Environment.h"
#include "Simul/Platform/DirectX9/SimulCloudRenderer.h"
#include "Simul/Platform/DirectX9/SimulLightningRenderer.h"
#include "Simul/Platform/DirectX9/SimulPrecipitationRenderer.h"
#include "Simul/Platform/DirectX9/Simul2DCloudRenderer.h"
#include "Simul/Platform/DirectX9/SimulSkyRenderer.h"
#include "Simul/Platform/DirectX9/SimulAtmosphericsRenderer.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/DirectX9/Macros.h"
#include "Simul/Platform/DirectX9/Resources.h"
#include <iomanip>

#define WRITE_PERFORMANCE_DATA
static simul::base::Timer timer;
using namespace simul;
using namespace dx9;
SimulWeatherRenderer::SimulWeatherRenderer(	simul::clouds::Environment *env,
										   simul::base::MemoryInterface *mem,
											bool usebuffer,int width,
											int height,bool sky,bool rain) :
	BaseWeatherRenderer(env,mem),
	m_pBufferVertexDecl(NULL),
	m_pd3dDevice(NULL),
	m_pBufferToScreenEffect(NULL),
	use_buffer(usebuffer),
	exposure(1.f),
	gamma(1.f/2.2f),
	simulSkyRenderer(NULL),
	simulCloudRenderer(NULL),
	simul2DCloudRenderer(NULL),
	simulPrecipitationRenderer(NULL),
	exposure_multiplier(1.f),
	show_rain(rain)
{
	//sky=rain=clouds2d=false;
	simul::sky::SkyKeyframer *sk=env->skyKeyframer;
	simul::clouds::CloudKeyframer *ck2d=env->cloud2DKeyframer;
	simul::clouds::CloudKeyframer *ck3d=env->cloudKeyframer;
	SetScreenSize(width,height);
	if(ShowSky)
	{
		simulSkyRenderer=new SimulSkyRenderer(sk);
		baseSkyRenderer=simulSkyRenderer;
	}
	
	{
		simulCloudRenderer=new SimulCloudRenderer(ck3d,mem);
		baseCloudRenderer=simulCloudRenderer;
		simulLightningRenderer=new SimulLightningRenderer(ck3d,sk);
		baseLightningRenderer=simulLightningRenderer;
		Restore3DCloudObjects();
	}
	
	{
		simul2DCloudRenderer=new Simul2DCloudRenderer(ck2d,mem);
		base2DCloudRenderer=simul2DCloudRenderer;
		Restore2DCloudObjects();
	}
	if(rain)
		simulPrecipitationRenderer=new SimulPrecipitationRenderer();
	simulAtmosphericsRenderer=new SimulAtmosphericsRenderer(mem);
	baseAtmosphericsRenderer=simulAtmosphericsRenderer;
	baseFramebuffer=&framebuffer;
	ConnectInterfaces();
}

/*
void SimulWeatherRenderer::ConnectInterfaces()
{
	if(!simulSkyRenderer)
		return;
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
		simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
	}
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
}
*/
void SimulWeatherRenderer::SetScreenSize(int w,int h)
{
	BufferWidth=w/Downscale;
	BufferHeight=h/Downscale;
}


bool SimulWeatherRenderer::Create(LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
/*	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simul2DCloudRenderer)
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
	if(simulSkyRenderer)
	{
		if(simulCloudRenderer)
			simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
	}
	if(simulCloudRenderer&&simulSkyRenderer)
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());*/
	HRESULT hr=S_OK;
	return (hr==S_OK);
}


bool SimulWeatherRenderer::Restore3DCloudObjects()
{
	HRESULT hr=S_OK;
	if(m_pd3dDevice)
	{
		if(simulCloudRenderer)
		{
			simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
		}
		if(simulPrecipitationRenderer)
			simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice);
		if(simulLightningRenderer)
			simulLightningRenderer->RestoreDeviceObjects(m_pd3dDevice);
	}
	return (hr==S_OK);
}
bool SimulWeatherRenderer::Restore2DCloudObjects()
{
	HRESULT hr=S_OK;
	if(m_pd3dDevice)
	{
		if(simul2DCloudRenderer)
			simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice);
	}
	return (hr==S_OK);
}

void SimulWeatherRenderer::RestoreDeviceObjects(void *dev)
{
	simul::base::Timer timer;
	timer.TimeSum=0;
	timer.StartTime();

	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	if(!m_pBufferToScreenEffect)
		V_CHECK(CreateDX9Effect(m_pd3dDevice,m_pBufferToScreenEffect,"gamma.fx"));
	SkyOverStarsTechnique		=m_pBufferToScreenEffect->GetTechniqueByName("simul_sky_over_stars");
	CloudBlendTechnique			=m_pBufferToScreenEffect->GetTechniqueByName("simul_cloud_blend");
	bufferTexture				=m_pBufferToScreenEffect->GetParameterByName(NULL,"hdrTexture");
	CreateBuffers();
	
	timer.UpdateTime();
	float create_buffers_time=timer.Time/1000.f;

	if(simulSkyRenderer)
		simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice);
	timer.UpdateTime();
	float sky_restore_time=timer.Time/1000.f;
	(Restore3DCloudObjects());
	timer.UpdateTime();
	float clouds_3d_restore_time=timer.Time/1000.f;
	(Restore2DCloudObjects());
	timer.UpdateTime();
	float clouds_2d_restore_time=timer.Time/1000.f;
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(dev);
	timer.UpdateTime();
	float atmospherics_restore_time=timer.Time/1000.f;
	std::cout<<std::setprecision(4)<<"RESTORE TIMINGS: create_buffers="<<create_buffers_time
		<<", sky="<<sky_restore_time<<", clouds_3d="<<clouds_3d_restore_time<<", clouds_2d="<<clouds_2d_restore_time
		<<", atmospherics="<<atmospherics_restore_time<<std::endl;

	UpdateSkyAndCloudHookup();
}

void SimulWeatherRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();
	if(simulLightningRenderer)
		simulLightningRenderer->InvalidateDeviceObjects();
	SAFE_RELEASE(m_pBufferVertexDecl);
	if(m_pBufferToScreenEffect)
        hr=m_pBufferToScreenEffect->OnLostDevice();
	SAFE_RELEASE(m_pBufferToScreenEffect);
	framebuffer.InvalidateDeviceObjects();
	lowdef_framebuffer.InvalidateDeviceObjects();
}

SimulWeatherRenderer::~SimulWeatherRenderer()
{
	InvalidateDeviceObjects();
	del(simulSkyRenderer,memoryInterface);
	del(simulCloudRenderer,memoryInterface);
	del(simul2DCloudRenderer,memoryInterface);
	del(simulPrecipitationRenderer,memoryInterface);
	del(simulAtmosphericsRenderer,memoryInterface);
}

void SimulWeatherRenderer::EnableRain(bool e)
{
	show_rain=e;
}

bool SimulWeatherRenderer::CreateBuffers()
{
	HRESULT hr=S_OK;
	framebuffer.SetWidthAndHeight(BufferWidth,BufferHeight);
	framebuffer.RestoreDeviceObjects(m_pd3dDevice);
	lowdef_framebuffer.SetWidthAndHeight(BufferWidth/2,BufferHeight/2);
	lowdef_framebuffer.RestoreDeviceObjects(m_pd3dDevice);
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  8, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT4		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITIONT,0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pBufferVertexDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pBufferVertexDecl);
	return (hr==S_OK);
}

bool SimulWeatherRenderer::RenderSky(void *context,float exposure,bool buffered,bool is_cubemap)
{
	PIXBeginNamedEvent(0xFF888888,"SimulWeatherRenderer::Render");
	BaseWeatherRenderer::RenderSky(context,exposure,buffered,is_cubemap);
	if(buffered&&!is_cubemap)
	{
#ifdef XBOX
		m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
#endif
		m_pBufferToScreenEffect->SetTechnique(SkyOverStarsTechnique);
		// When we put the sky buffer to the screen we want to NOT change the destination alpha, because we may
		// have this set aside for depth!
		static int u=7;
		m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,u);
		RenderBufferToScreen((LPDIRECT3DTEXTURE9)framebuffer.GetColorTex());
		m_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,15);
	}
	return true;
}

void SimulWeatherRenderer::RenderLightning(void *context,int viewport_id)
{
	if(simulCloudRenderer&&simulLightningRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		return simulLightningRenderer->Render(context);
}

void SimulWeatherRenderer::RenderPrecipitation(void *context)
{
	if(simulPrecipitationRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible()) 
		simulPrecipitationRenderer->Render(context);
}

void SimulWeatherRenderer::RenderLateCloudLayer(void *context,float exposure,bool buf,int viewport_id,const simul::sky::float4 &relativeViewportTextureRegionXYWH)
{
	if(!RenderCloudsLate||!simulCloudRenderer->GetCloudKeyframer()->GetVisible())
		return;
	HRESULT hr=S_OK;
	LPDIRECT3DSURFACE9	m_pOldRenderTarget=NULL;
	LPDIRECT3DSURFACE9	m_pOldDepthSurface=NULL;
	if(buf)
	{
		framebuffer.Activate(NULL);
		static float depth_start=1.f;
		hr=m_pd3dDevice->Clear(0L,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,depth_start,0L);
	}
	
	if(simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{	
		PIXWrapper(D3DCOLOR_RGBA(255,0,0,255),"CLOUDS")
		{
			simulCloudRenderer->Render(context,exposure,false,0,false,true,viewport_id,relativeViewportTextureRegionXYWH);
		}
	}
	
	static bool do_fx=true;
	if(do_fx)
	if(simulAtmosphericsRenderer&&simulCloudRenderer&&simulCloudRenderer->GetCloudKeyframer()->GetVisible())
	{
		float str=simulCloudRenderer->GetCloudInterface()->GetHumidity();
		static float gr_start=0.65f;
		static float gr_strength=0.5f;
		str-=gr_start;
		str/=(1.f-gr_start);
		if(str<0.f)
			str=0.f;
		if(str>1.f)
			str=1.f;
		str*=gr_strength;
		if(str>0&&simulAtmosphericsRenderer->GetShowGodrays())
			simulAtmosphericsRenderer->RenderGodRays(str);
		simulAtmosphericsRenderer->RenderAirglow();
	}
	if(buf)
	{
		m_pBufferToScreenEffect->SetTechnique(CloudBlendTechnique);
		{
			framebuffer.Deactivate(NULL);
	#ifdef XBOX
			m_pd3dDevice->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, framebuffer.hdr_buffer_texture, NULL, 0, 0, NULL, 0.0f, 0, NULL);
	#endif
			RenderBufferToScreen((LPDIRECT3DTEXTURE9)framebuffer.GetColorTex());
		}
	}
	SAFE_RELEASE(m_pOldRenderTarget);
	SAFE_RELEASE(m_pOldDepthSurface);
}

bool SimulWeatherRenderer::RenderBufferToScreen(LPDIRECT3DTEXTURE9 texture)
{
	HRESULT hr=m_pBufferToScreenEffect->SetTexture(bufferTexture,texture);
	hr=DrawFullScreenQuad(m_pd3dDevice,m_pBufferToScreenEffect);
	return (hr==S_OK);
}

#ifdef XBOX
void SimulWeatherRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(v,p);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(v,p);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(v,p);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(v,p);
	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(v,p);
}
#endif

void SimulWeatherRenderer::PreRenderUpdate(void *context,float dt)
{
	BaseWeatherRenderer::PreRenderUpdate(context);
	//if(baseCloudRenderer&&baseAtmosphericsRenderer)
	{
		//void **c=baseCloudRenderer->GetCloudTextures();
	/*	baseAtmosphericsRenderer->SetCloudProperties(c[0],c[1],
			baseCloudRenderer->GetCloudScales(),
			baseCloudRenderer->GetCloudOffset(),
			baseCloudRenderer->GetInterpolation());*/
	}
	if(simulPrecipitationRenderer)
	{
		simulPrecipitationRenderer->Update(dt);
		if(simulCloudRenderer&&environment->cloudKeyframer->GetVisible())
		{
			simulPrecipitationRenderer->SetWind(environment->cloudKeyframer->GetWindSpeed(),environment->cloudKeyframer->GetWindHeadingDegrees());
		#ifndef XBOX
			float cam_pos[3];
			D3DXMATRIX view;
			m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
		#endif
			GetCameraPosVector(view,simulCloudRenderer->IsYVertical(),cam_pos);
			simulPrecipitationRenderer->SetIntensity(environment->cloudKeyframer->GetPrecipitationIntensity(cam_pos));
		}
		else
		{
			simulPrecipitationRenderer->SetIntensity(0.f);
			simulPrecipitationRenderer->SetWind(0,0);
		}
		if(simulSkyRenderer)
		{
			simul::sky::float4 l=simulSkyRenderer->GetLightColour();
			simulPrecipitationRenderer->SetLightColour((const float*)(l));
		}
	}
}

SimulSkyRenderer *SimulWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer;
}

SimulCloudRenderer *SimulWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer;
}

Simul2DCloudRenderer *SimulWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer;
}

SimulPrecipitationRenderer *SimulWeatherRenderer::GetPrecipitationRenderer()
{
	return simulPrecipitationRenderer;
}

SimulAtmosphericsRenderer *SimulWeatherRenderer::GetAtmosphericsRenderer()
{
	return simulAtmosphericsRenderer;
}
float SimulWeatherRenderer::GetTotalBrightness() const
{
	return exposure*exposure_multiplier;
} 

const char *SimulWeatherRenderer::GetDebugText() const
{
	static char debug_text[256];
	if(simulSkyRenderer)
		sprintf_s(debug_text,256,"%s",simulSkyRenderer->GetDebugText());
/*	if(simulCloudRenderer)
		sprintf_s(debug_text,256,"%s\ntotal %3.3g ms, clouds %3.3g ms, sky %3.3g ms, final %3.3g",
			simulCloudRenderer->GetDebugText(),total_timing,cloud_timing,sky_timing,final_timing);*/
	return debug_text;
}
