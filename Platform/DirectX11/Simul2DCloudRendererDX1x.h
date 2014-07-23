// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// Simul2DCloudRendererDX11.h A renderer for 2D cloud layers.

#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#include "Simul/Clouds/Base2DCloudRenderer.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/HLSL/CppHlsl.hlsl"

namespace simul
{
	namespace dx11
	{
		//! A renderer for 2D cloud layers, e.g. cirrus clouds.
		class Simul2DCloudRendererDX11: public simul::clouds::Base2DCloudRenderer
		{
		public:
			Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck2d,simul::base::MemoryInterface *mem);
			virtual ~Simul2DCloudRendererDX11();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void RecompileShaders();
			void InvalidateDeviceObjects();
			void PreRenderUpdate(crossplatform::DeviceContext &deviceContext);
			bool Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap,crossplatform::NearFarPass nearFarPass
				,crossplatform::Texture *depth_tex,bool write_alpha
				,const simul::sky::float4& viewportTextureRegionXYWH
				,const simul::sky::float4& );
			void RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height);
			void RenderAuxiliaryTextures(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height);
			void SetIlluminationTexture(crossplatform::Texture *i);
			void SetLightTableTexture(crossplatform::Texture *l);
			void SetWindVelocity(float x,float y);
		protected:
			void RenderDetailTexture(crossplatform::DeviceContext &deviceContext);
			void EnsureCorrectTextureSizes();
			virtual void EnsureTexturesAreUpToDate(crossplatform::DeviceContext &deviceContext);
			void EnsureTextureCycle();
			void EnsureCorrectIlluminationTextureSizes(){}
			void EnsureIlluminationTexturesAreUpToDate(){}
			void CreateNoiseTexture(crossplatform::DeviceContext &deviceContext){}
			crossplatform::Effect*		effect;
			crossplatform::EffectTechnique*		msaaTechnique;
			crossplatform::EffectTechnique*		technique;
			ID3D11Buffer*				vertexBuffer;
			ID3D11Buffer*				indexBuffer;
			ID3D11InputLayout*			inputLayout;
			
			crossplatform::ConstantBuffer<Cloud2DConstants>	cloud2DConstants;
			crossplatform::ConstantBuffer<Detail2DConstants>	detail2DConstants;
			int num_indices;

			simul::crossplatform::Texture *coverage;
			simul::crossplatform::Texture *detail;
			simul::crossplatform::Texture *noise;
			simul::crossplatform::Texture *dens;
		};
	}
}