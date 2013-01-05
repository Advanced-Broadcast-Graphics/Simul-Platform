#pragma once

#ifndef D3DCALLBACKINTERFACE_H
#define D3DCALLBACKINTERFACE_H
// Direct3D includes
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
class Direct3D9CallbackInterface
{
public:
	virtual bool	IsDeviceAcceptable(D3DCAPS9* pCaps,D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat,bool bWindowed)=0;
	virtual bool    ModifyDeviceSettings(struct DXUTDeviceSettings* pDeviceSettings)=0;
	virtual HRESULT OnCreateDevice(IDirect3DDevice9* pd3dDevice,const D3DSURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual HRESULT OnResetDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc)=0;
	virtual void    OnFrameMove(double fTime, float fTimeStep)=0;
	virtual void    OnFrameRender(IDirect3DDevice9* pd3dDevice, double fTime, float fTimeStep)=0;
	virtual LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing)=0;
	virtual void    OnLostDevice()=0;
	virtual void    OnDestroyDevice()=0;
	virtual const TCHAR *GetDebugText() const=0;
};
#endif

#include "Simul/Base/PropertyMacros.h"
#include "Simul/Graph/StandardNodes/ShowProgressInterface.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Platform/DirectX9/Export.h"
#include "Simul/Platform/DirectX9/GpuCloudGenerator.h"
#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace sky
	{
		class BaseGpuSkyGenerator;
	}
	namespace clouds
	{
		class Environment;
	}
	namespace camera
	{
		class Camera;
	}
}
class SimulWeatherRenderer;
class SimulHDRRenderer;
class SimulTerrainRenderer;
class SimulOpticsRendererDX9;

class SIMUL_DIRECTX9_EXPORT Direct3D9Renderer
	:public Direct3D9CallbackInterface
	,public simul::graph::meta::Group
{
public:
	Direct3D9Renderer(simul::clouds::Environment *env,int w,int h);
	virtual ~Direct3D9Renderer();
	META_BeginProperties
		META_ValueProperty(bool,ShowFlares,"Whether to draw light flares around the sun and moon.")

		META_ValueProperty(bool,ShowCloudCrossSections,"Show cross-sections of the cloud volumes as an overlay.")
		META_ValueProperty(bool,ShowFades,"Show the fade textures as an overlay.")
		META_ValueProperty(bool,ShowTerrain,"Whether to draw the terrain.")
		META_ValueProperty(bool,UseHdrPostprocessor,"Whether to apply post-processing for exposure and gamma-correction using a post-processing renderer.")
		META_ValueProperty(bool,ShowMap,"Show the terrain map as an overlay.")
		META_ValueProperty(bool,ShowLightVolume,"Show the cloud light volume as a wireframe box.")
		META_ValueProperty(bool,CelestialDisplay,"Show geographical and sidereal overlay.")
		META_ValueProperty(bool,ShowWater,"Show water surfaces.")
		META_ValueProperty(bool,ReverseDepth,"Reverse the direction of the depth (Z) buffer, so that depth 0 is the far plane.")
	META_EndProperties
	SimulWeatherRenderer *GetSimulWeatherRenderer(){return simulWeatherRenderer.get();}
	SimulTerrainRenderer *GetSimulTerrainRenderer(){return simulTerrainRenderer.get();}
	SimulHDRRenderer *GetSimulHDRRenderer(){return simulHDRRenderer.get();}
	void SetShowOSD(bool s);
	void SetCamera(simul::camera::Camera *c);
//Direct3D9CallbackInterface:
	bool	IsDeviceAcceptable(D3DCAPS9* pCaps, D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat, bool bWindowed);
	bool    ModifyDeviceSettings(struct DXUTDeviceSettings* pDeviceSettings);
	HRESULT OnCreateDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc);
	HRESULT OnResetDevice(IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc);
	void    OnFrameMove(double fTime, float fTimeStep);
	void    OnFrameRender(IDirect3DDevice9* pd3dDevice, double fTime, float fTimeStep);
	LRESULT MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing);
	void    KeyboardProc(UINT nChar, bool bKeyDown, bool bAltDown);
	void    OnLostDevice();
	void    OnDestroyDevice();

	void	SetYVertical(bool y);
	void	RecompileShaders();
	simul::dx9::GpuCloudGenerator *GetGpuCloudGenerator(){return &gpuCloudGenerator;}
	simul::sky::BaseGpuSkyGenerator *GetGpuSkyGenerator(){return NULL;}
protected:
	HRESULT RestoreDeviceObjects(IDirect3DDevice9* pDevice);
	simul::camera::Camera *camera;
	float aspect;
	bool device_reset;
	bool y_vertical;
	bool show_osd;
	float framerate;
	simul::base::SmartPtr<SimulOpticsRendererDX9> simulOpticsRenderer;
	simul::base::SmartPtr<SimulWeatherRenderer> simulWeatherRenderer;
	simul::base::SmartPtr<SimulTerrainRenderer> simulTerrainRenderer;
	simul::base::SmartPtr<SimulHDRRenderer> simulHDRRenderer;
	const TCHAR *GetDebugText() const;
	int width,height;
	float time_mult;
	simul::dx9::GpuCloudGenerator gpuCloudGenerator;
};

#pragma warning(pop)