// Copyright (c) 2007-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once

#include <d3dx9.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx11effect.h>
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Base/Referenced.h"
#include "Simul/Base/PropertyMacros.h"
#pragma warning(push)
#pragma warning(disable:4251)
//! A class that provides gamma-correction of an HDR render to the screen.
SIMUL_DIRECTX11_EXPORT_CLASS SimulHDRRendererDX1x: public simul::base::Referenced
{
public:
	SimulHDRRendererDX1x(int w,int h);
	virtual ~SimulHDRRendererDX1x();
	META_BeginProperties
		META_ValueProperty(float,Gamma,"")
		META_ValueProperty(float,Exposure,"")
	META_EndProperties
	void SetBufferSize(int w,int h);
	// Standard d3d object interface functions
	//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
	void RestoreDeviceObjects(void *x);
	//! Call this when the device has been lost.
	void InvalidateDeviceObjects();
	//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
	bool StartRender(void *context);
	//! ApplyFade: call this after rendering the solid stuff, before rendering transparent and background imagery.
	bool ApplyFade();
	//! FinishRender: wraps up rendering to the HDR target, and then uses tone mapping to render this HDR image to the screen. Call at the end of the frame's rendering.
	bool FinishRender(void *context);
	//! Create the glow texture that will be overlaid due to strong lights.
	void RenderGlowTexture(void *context);
	//! Get the current debug text as a c-string pointer.
	const char *GetDebugText() const;
	//! Get a timing value for debugging.
	float GetTiming() const;

	void RecompileShaders();
protected:
	bool Destroy();
	FramebufferDX1x framebuffer;
	FramebufferDX1x glow_fb;
	int Width,Height;
	ID3D1xDevice*						m_pd3dDevice;
	ID3D1xBuffer*						m_pVertexBuffer;

	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	ID3D1xEffect*						m_pTonemapEffect;
	ID3D1xEffectTechnique*				TonemapTechnique;
	ID3D1xEffectTechnique*				glowTechnique;
	ID3D1xEffectScalarVariable*			Exposure_;
	ID3D1xEffectScalarVariable*			Gamma_;
	ID3D1xEffectMatrixVariable*			worldViewProj;
	ID3D1xEffectShaderResourceVariable*	imageTexture;

	ID3D1xEffect*						m_pGaussianEffect;
	ID3D1xEffectTechnique*				gaussianRowTechnique;
	ID3D1xEffectTechnique*				gaussianColTechnique;

	float timing;
private:
	simul::dx11::ComputableTexture					glowTexture;
};
#pragma warning(pop)