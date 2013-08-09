#pragma once
// Direct3D includes
#ifdef DX10
	#include <D3D10.h>
	#include <d3dx10.h>
#else
	#include <d3d11.h>
	#include <d3dx11.h>
#endif
#include <Simul\External\DirectX\DXUT11\Core\dxut.h>
#include <Simul\External\DirectX\DXUT11\Core\dxutdevice11.h>
#include <dxerr.h>

#include "Simul/Platform/DirectX11/Direct3D11CallbackInterface.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/GpuSkyGenerator.h"
#include "Simul/Platform/DirectX11/FramebufferCubemapDX1x.h"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace camera
	{
		class Camera;
	}
	namespace clouds
	{
		class Environment;
	}
}
namespace simul
{
	namespace dx11
	{
		class SimulWeatherRendererDX1x;
		class SimulHDRRendererDX1x;
		class SimulTerrainRendererDX1x;
		class SimulOpticsRendererDX1x;

		//! A renderer for DirectX11. Use this class as a guide to implementing your own rendering in DX11.
		class SIMUL_DIRECTX11_EXPORT Direct3D11Renderer
			:public Direct3D11CallbackInterface
			,public simul::graph::meta::Group
		{
		public:
			//! Constructor - pass a pointer to your Environment, and either an implementation of MemoryInterface, or NULL.
			Direct3D11Renderer(simul::clouds::Environment *env,simul::base::MemoryInterface *m,int w,int h);
			virtual ~Direct3D11Renderer();
			META_BeginProperties
				META_ValueProperty(bool,ShowFlares				,"Whether to draw light flares around the sun and moon.")
				META_ValueProperty(bool,ShowCloudCrossSections	,"Show the cloud textures as an overlay.")
				META_ValueProperty(bool,Show2DCloudTextures		,"Show the 2D cloud textures as an overlay.")
				META_ValueProperty(bool,ShowFades				,"Show the fade textures as an overlay.")
				META_ValueProperty(bool,ShowTerrain				,"Whether to draw the terrain.")
				META_ValueProperty(bool,ShowMap					,"Show the terrain map as an overlay.")
				META_ValueProperty(bool,UseHdrPostprocessor		,"Whether to apply post-processing for exposure and gamma-correction using a post-processing renderer.")
				META_ValueProperty(bool,UseSkyBuffer			,"Render the sky to a low-res buffer to increase performance.")
				META_ValueProperty(bool,ShowLightVolume			,"Show the cloud light volume as a wireframe box.")
				META_ValueProperty(bool,CelestialDisplay		,"Show geographical and sidereal overlay.")
				META_ValueProperty(bool,ShowWater				,"Show water surfaces.")
				META_ValueProperty(bool,MakeCubemap				,"Render a cubemap each frame.")
				META_ValuePropertyWithSetCall(bool,ReverseDepth,ReverseDepthChanged,"Reverse the direction of the depth (Z) buffer, so that depth 0 is the far plane.")
				META_ValueProperty(bool,ShowOSD					,"Show debug display.")
				META_ValueProperty(float,Exposure				,"A linear multiplier for rendered brightness.")
			META_EndProperties
			bool IsEnabled()const{return enabled;}
			class SimulWeatherRendererDX1x *GetSimulWeatherRenderer()
			{
				return simulWeatherRenderer.get();
			}
			class SimulHDRRendererDX1x *GetSimulHDRRenderer()
			{
				return simulHDRRenderer.get();
			}
			void SetCamera(simul::camera::Camera *c)
			{
				camera=c;
			}
			void	RecompileShaders();
			void	RenderCubemap(ID3D11DeviceContext* pd3dImmediateContext,D3DXVECTOR3 cam_pos);
			// D3D11CallbackInterface
			virtual bool	IsD3D11DeviceAcceptable(	const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,DXGI_FORMAT BackBufferFormat,bool bWindowed);
			virtual bool	ModifyDeviceSettings(		DXUTDeviceSettings* pDeviceSettings);
			virtual HRESULT	OnD3D11CreateDevice(		ID3D11Device* pd3dDevice,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
			virtual HRESULT	OnD3D11ResizedSwapChain(	ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
			virtual void	OnD3D11FrameRender(			ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep);
			virtual void	OnD3D11LostDevice();
			virtual void	OnD3D11DestroyDevice();
			virtual void	OnD3D11ReleasingSwapChain();
			virtual bool	OnDeviceRemoved();
			virtual void    OnFrameMove(double fTime,float fTimeStep);
			virtual const	char *GetDebugText() const;
		protected:
			void ReverseDepthChanged();
			bool enabled;
			simul::camera::Camera *camera;
			simul::base::SmartPtr<SimulOpticsRendererDX1x>	simulOpticsRenderer;
			simul::base::SmartPtr<SimulWeatherRendererDX1x>	simulWeatherRenderer;
			simul::base::SmartPtr<SimulHDRRendererDX1x>		simulHDRRenderer;
			simul::base::SmartPtr<SimulTerrainRendererDX1x>	simulTerrainRenderer;
			int ScreenWidth,ScreenHeight;
			// A depth-only FB to make sure we have a readable depth texture.
			simul::dx11::Framebuffer depthFramebuffer;
			simul::dx11::Framebuffer cubemapDepthFramebuffer;
			FramebufferCubemapDX1x	framebuffer_cubemap;
		};
	}
}
#pragma warning(pop)