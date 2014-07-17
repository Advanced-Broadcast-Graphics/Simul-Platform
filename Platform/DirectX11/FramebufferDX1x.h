#pragma once
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif
#include "Simul/Platform/DirectX11/MacrosDx1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"

namespace simul
{
	namespace dx11
	{
		//! A DirectX 11 framebuffer class.
		SIMUL_DIRECTX11_EXPORT_CLASS Framebuffer : public crossplatform::BaseFramebuffer
		{
		public:
			Framebuffer(int w=0,int h=0);
			virtual ~Framebuffer();
			void SetWidthAndHeight(int w,int h);
			void SetFormat(int f);
			void SetDepthFormat(int f);
			bool IsValid() const;
			void SetAntialiasing(int a)
			{
				if(numAntialiasingSamples!=a)
				{
					numAntialiasingSamples=a;
					InvalidateDeviceObjects();
				}
			}
			void SetGenerateMips(bool);
			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(crossplatform::RenderPlatform	*renderPlatform);
			bool CreateBuffers();
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			//! StartRender: sets up the rendertarget for HDR, and make it the current target. Call at the start of the frame's rendering.
			void Activate(crossplatform::DeviceContext &deviceContext);
			void ActivateColour(crossplatform::DeviceContext &deviceContext,const float viewportXYWH[4]);
			void ActivateDepth(crossplatform::DeviceContext &deviceContext);
			void ActivateViewport(crossplatform::DeviceContext &deviceContext, float viewportX, float viewportY, float viewportW, float viewportH );
			void ActivateColour(crossplatform::DeviceContext &context);
			void Deactivate(void *context);
			void DeactivateDepth(void *context);
			void Clear(void *context,float,float,float,float,float,int mask=0);
			void ClearDepth(void *context,float);
			void ClearColour(void* context, float, float, float, float );
			ID3D11ShaderResourceView *GetBufferResource()
			{
				return buffer_texture.shaderResourceView;
			}
			void* GetColorTex()
			{
				return (void*)buffer_texture.shaderResourceView;
			}
			//! Get the API-dependent pointer or id for the depth buffer target.
			ID3D11ShaderResourceView* GetDepthSRV()
			{
				return buffer_depth_texture.shaderResourceView;
			}
			ID3D11Texture2D* GetColorTexture()
			{
				return (ID3D11Texture2D*)buffer_texture.texture;
			}
			ID3D11Texture2D* GetDepthTexture2D()
			{
				return (ID3D11Texture2D*)buffer_depth_texture.texture;
			}
			//! Copy from the rt to the given target memory. If not starting at the top of the texture (start_texel>0), the first byte written
			//! is at \em target, which is the address to copy the given chunk to, not the base address of the whole in-memory texture.
			void CopyToMemory(void *context,void *target,int start_texel=0,int texels=0);
			void GetTextureDimensions(const void* tex, unsigned int& widthOut, unsigned int& heightOut) const;
			Texture *GetTexture()
			{
				return &buffer_texture;
			}
			Texture *GetDepthTexture()
			{
				return &buffer_depth_texture;
			}
		protected:
			DXGI_FORMAT target_format;
			DXGI_FORMAT depth_format;
			bool Destroy();
			ID3D11Device*						m_pd3dDevice;

		public:
			//ID3D1xRenderTargetView*				m_pHDRRenderTarget;
			ID3D1xDepthStencilView*				m_pBufferDepthSurface;
		protected:
			ID3D11Texture2D *stagingTexture;	// Only initialized if CopyToMemory is invoked.
			
			ID3D1xRenderTargetView*				m_pOldRenderTarget;
			ID3D1xDepthStencilView*				m_pOldDepthSurface;
			D3D11_VIEWPORT						m_OldViewports[16];
			unsigned							num_OldViewports;
			//! The texture the scene is rendered to.
		public:
			dx11::Texture						buffer_texture;
		protected:
			//! The depth buffer.
			dx11::Texture						buffer_depth_texture;
			bool IsDepthFormatOk(DXGI_FORMAT DepthFormat, DXGI_FORMAT AdapterFormat, DXGI_FORMAT BackBufferFormat);
			ID3D1xRenderTargetView* MakeRenderTarget(const ID3D1xTexture2D* pTexture);
			float timing;
			bool GenerateMips;
			void SaveOldRTs(void *context);
			void SetViewport(void *context,float X,float Y,float W,float H,float Z,float D);
		};
	}
}
