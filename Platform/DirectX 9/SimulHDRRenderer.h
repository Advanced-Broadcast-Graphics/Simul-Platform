// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Base/Referenced.h"
#include "Simul/Platform/DirectX 9/Export.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
typedef long HRESULT;
SIMUL_DIRECTX9_EXPORT_CLASS SimulHDRRenderer: public simul::base::Referenced
{
public:
	SimulHDRRenderer(int width,int height);
	virtual ~SimulHDRRenderer();
	//standard d3d object interface functions

	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	bool RestoreDeviceObjects(void *pd3dDevice);
	//! Call this when the device has been lost.
	bool InvalidateDeviceObjects();
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	bool StartRender();
	//! ApplyFade: call this after rendering the solid stuff, before rendering transparent and background imagery.
	bool ApplyFade();
	//! FinishRender: wraps up rendering to the HDR target, and then uses tone mapping to render this HDR image to the screen. Call at the end of the frame's rendering.
	bool FinishRender();

	//! Set the exposure - a brightness factor.
	void SetExposure(float ex);
	//! Set the gamma - a tone-mapping factor.
	void SetGamma(float g);
	//! Get the current debug text as a c-string pointer.
	const char *GetDebugText() const;
	//! Get a timing value for debugging.
	float GetTiming() const;
	//! Set the atmospherics renderer - null means no post-process fade.
	void SetAtmospherics(class SimulAtmosphericsInterface *a){atmospherics=a;}
	void SetBufferSize(int w,int h);

protected:
	//! The size of the 2D buffer the sky is rendered to.
	int BufferWidth,BufferHeight;
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pBufferVertexDecl;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	LPD3DXEFFECT			m_pTonemapEffect;
	D3DXHANDLE				ToneMapTechnique;
	D3DXHANDLE				ToneMapZWriteTechnique;
	D3DXHANDLE				Exposure;
	D3DXHANDLE				Gamma;
	D3DXHANDLE				hdrTexture;

	LPDIRECT3DSURFACE9		m_pHDRRenderTarget;
	LPDIRECT3DSURFACE9		m_pFadedRenderTarget;
	LPDIRECT3DSURFACE9		m_pBufferDepthSurface;
	LPDIRECT3DSURFACE9		m_pOldRenderTarget;
	LPDIRECT3DSURFACE9		m_pOldDepthSurface;

	//! The texture the scene is rendered to.
	LPDIRECT3DTEXTURE9		hdr_buffer_texture;
	//! The texture the fade is applied to.
	LPDIRECT3DTEXTURE9		faded_texture;
	//! The depth buffer.
	LPDIRECT3DTEXTURE9		buffer_depth_texture;

	// Optional stuff for full HDR with brightpass etc:
	D3DXHANDLE				BrightpassTechnique;
	D3DXHANDLE				BlurTechnique;
	LPDIRECT3DSURFACE9		m_pBrightpassRenderTarget;
	LPDIRECT3DTEXTURE9		brightpass_buffer_texture;
	LPDIRECT3DTEXTURE9		bloom_texture;
	LPDIRECT3DTEXTURE9		last_texture;
	D3DXHANDLE				brightpassThreshold;
	D3DXHANDLE				brightPassOffsets;
	D3DXHANDLE				bloomTexture;
	D3DXHANDLE				bloomOffsets;
	D3DXHANDLE				bloomWeights;
		
	bool IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat);
	bool CreateBuffers();
	bool RenderBufferToCurrentTarget(int w,int h,bool do_tonemap);
	class SimulSkyRenderer *simulSkyRenderer;
	class SimulCloudRenderer *simulCloudRenderer;
	class Simul2DCloudRenderer *simul2DCloudRenderer;
	class SimulPrecipitationRenderer *simulPrecipitationRenderer;
	float							exposure;
	float							gamma;
	LPDIRECT3DSURFACE9 MakeRenderTarget(const LPDIRECT3DTEXTURE9 pTexture);
	float timing;
	float exposure_multiplier;
	bool RenderBrightpass();
	bool RenderBloom();
	class SimulAtmosphericsInterface *atmospherics;
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif