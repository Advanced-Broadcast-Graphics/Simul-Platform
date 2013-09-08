// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Sky/Float4.h"
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
typedef long HRESULT;
namespace simul
{
	namespace sky
	{
		class BaseSkyInterface;
	}
	namespace clouds
	{
		class LightningRenderInterface;
	}
}

class SimulAtmosphericsInterface
{
public:
	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	virtual void RestoreDeviceObjects(void *pd3dDevice)=0;
	//! Call this when the device has been lost.
	virtual void InvalidateDeviceObjects()=0;
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	virtual bool Render()=0;
	//virtual void SetInputTextures(LPDIRECT3DTEXTURE9 image,LPDIRECT3DTEXTURE9 depth)=0;
};

// A class that takes an image buffer and a z-buffer and applies atmospheric fading.
class SimulAtmosphericsRenderer : public SimulAtmosphericsInterface, public simul::sky::BaseAtmosphericsRenderer
{
public:
	SimulAtmosphericsRenderer(simul::base::MemoryInterface *m);
	virtual ~SimulAtmosphericsRenderer();
	//standard d3d object interface functions

	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	void RestoreDeviceObjects(void *pd3dDevice);
	void RecompileShaders();
	//! Call this when the device has been lost.
	void InvalidateDeviceObjects();
	//! StartRender: sets up the rendertarget for atmospherics, and make it the current target. Call at the start of the frame's rendering.
	void StartRender(void *context);
	void FinishRender(void *context);
	//! Not implemented for DirectX 9.
	void RenderAsOverlay(void *,const void *,float,const simul::sky::float4 &){}
	void *GetDepthAlphaTexture()
	{
		return (void*)input_texture;
	}
	//! Set properties for rendering cloud godrays.
	void SetCloudProperties(void* c1,void* c2,
							const float *cloudscales,
							const float *cloudoffset,
							float interp);
	//! Render godrays
	bool RenderGodRays(float strength);
	//! Set properties for rendering lightning airglow.
	void SetLightningProperties(void *tex,simul::clouds::LightningRenderInterface *lri);
	//! Render airglow due to lightning
	bool RenderAirglow();
#ifdef XBOX
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
#endif
	void SetDistanceTexture(void* t)
	{
		max_distance_texture=(LPDIRECT3DTEXTURE9)t;
	}
	void SetLossTexture(void* t)
	{
		loss_texture=(LPDIRECT3DBASETEXTURE9)t;
	}
	void SetInscatterTextures(void* t,void *s,void *)
	{
		inscatter_texture=(LPDIRECT3DBASETEXTURE9)t;
		skylight_texture=(LPDIRECT3DBASETEXTURE9)s;
	}
	void SetCloudsTexture(void* t)
	{
		clouds_texture=(LPDIRECT3DBASETEXTURE9)t;
	}
	void SetFadeInterpolation(float s)
	{
		fade_interp=s;
	}
	// implementing BaseAtmosphericsRenderer
	void SetYVertical(bool y)
	{
		y_vertical=y;
	}
	void SetBufferSize(int,int){}
protected:
	bool Render();
	bool DrawScreenQuad();
	bool y_vertical;
	bool use_3d_fades;
	float altitude_tex_coord;
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	vertexDecl;
	D3DXMATRIX						world,view,proj;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	LPD3DXEFFECT					effect;
	D3DXHANDLE						technique;
	// Variables for this effect:
	D3DXHANDLE						invViewProj;
	D3DXHANDLE						altitudeTexCoord;
	D3DXHANDLE						lightDir;
	D3DXHANDLE						sunDir;
	D3DXHANDLE						mieRayleighRatio;
	D3DXHANDLE						hazeEccentricity;
	D3DXHANDLE						fadeInterp;
	D3DXHANDLE						imageTexture;
	//D3DXHANDLE						depthTexture;
	D3DXHANDLE						maxDistanceTexture;
	D3DXHANDLE						lossTexture1;
	D3DXHANDLE						inscatterTexture1;
	D3DXHANDLE						skylightTexture;

	D3DXHANDLE						heightAboveFogLayer;
	D3DXHANDLE						fogColour;
	D3DXHANDLE						fogExtinction;
	D3DXHANDLE						fogDensity;
	// For godrays:
	D3DXHANDLE						godRaysTechnique;
	D3DXHANDLE						cloudTexture1;
	D3DXHANDLE						cloudTexture2;
	D3DXHANDLE						cloudScales;
	D3DXHANDLE						cloudOffset;
	D3DXHANDLE						lightColour;
	D3DXHANDLE						eyePosition;
	D3DXHANDLE						cloudInterp;
	LPDIRECT3DBASETEXTURE9			cloud_texture1;
	LPDIRECT3DBASETEXTURE9			cloud_texture2;
	simul::sky::float4				cloud_scales;
	simul::sky::float4				cloud_offset;
	float							cloud_interp;

	LPDIRECT3DBASETEXTURE9			loss_texture;
	LPDIRECT3DBASETEXTURE9			inscatter_texture;
	LPDIRECT3DBASETEXTURE9			skylight_texture;
	LPDIRECT3DBASETEXTURE9			clouds_texture;

	LPDIRECT3DSURFACE9				m_pRenderTarget;
	LPDIRECT3DSURFACE9				m_pBufferDepthSurface;
	LPDIRECT3DSURFACE9				m_pOldRenderTarget;
	LPDIRECT3DSURFACE9				m_pOldDepthSurface;

	// For lightning airglow
	D3DXHANDLE						airglowTechnique;
	LPDIRECT3DBASETEXTURE9			lightning_illumination_texture;
	simul::sky::float4				lightning_multipliers;
	simul::sky::float4				illumination_scales;
	simul::sky::float4				illumination_offset;
	simul::sky::float4				lightning_colour;
	D3DXHANDLE						lightningIlluminationTexture;
	D3DXHANDLE						lightningMultipliers;
	D3DXHANDLE						lightningColour;
	D3DXHANDLE						illuminationOrigin;
	D3DXHANDLE						illuminationScales;
	D3DXHANDLE						texelOffsets;

	//! The depth buffer.
	LPDIRECT3DTEXTURE9				depth_texture;
	//! The un-faded image buffer.
	LPDIRECT3DTEXTURE9				input_texture;
	//! 
	LPDIRECT3DTEXTURE9				max_distance_texture;

	float fade_interp;
};