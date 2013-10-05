#define NOMINMAX
#include "Simul/Platform/DirectX11/Direct3D11Renderer.h"
#include "Simul/Platform/DirectX11/SimulWeatherRendererDX11.h"
#include "Simul/Platform/DirectX11/SimulTerrainRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulCloudRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulHDRRendererDX1x.h"
#include "Simul/Platform/DirectX11/Simul2DCloudRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulSkyRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulAtmosphericsRendererDX1x.h"
#include "Simul/Platform/DirectX11/SimulOpticsRendererDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/SaveTextureDx1x.h"

#include "Simul/Camera/Camera.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/pi.h"

using namespace simul;
using namespace dx11;

Direct3D11Renderer::Direct3D11Renderer(simul::clouds::Environment *env,simul::base::MemoryInterface *m,int w,int h):
		camera(NULL)
		,ShowCloudCrossSections(false)
		,ShowFlares(true)
		,Show2DCloudTextures(false)
		,ShowFades(false)
		,ShowTerrain(true)
		,ShowMap(false)
		,UseHdrPostprocessor(true)
		,UseSkyBuffer(true)
		,ShowLightVolume(false)
		,CelestialDisplay(false)
		,ShowWater(true)
		,MakeCubemap(true)
		,ReverseDepth(true)
		,ShowOSD(false)
		,Exposure(1.0f)
		,Antialiasing(1)
		,enabled(false)
		,m_pd3dDevice(NULL)
		,mixedResolutionEffect(NULL)
		,simulOpticsRenderer(NULL)
		,simulWeatherRenderer(NULL)
		,simulHDRRenderer(NULL)
		,simulTerrainRenderer(NULL)
		,memoryInterface(m)
{
	simulWeatherRenderer	=::new(memoryInterface) SimulWeatherRendererDX11(env,memoryInterface);
	simulHDRRenderer		=::new(memoryInterface) SimulHDRRendererDX1x(128,128);
	simulOpticsRenderer		=::new(memoryInterface) SimulOpticsRendererDX1x(memoryInterface);
	simulTerrainRenderer	=::new(memoryInterface) SimulTerrainRendererDX1x(memoryInterface);
	simulTerrainRenderer->SetBaseSkyInterface(env->skyKeyframer);
	ReverseDepthChanged();
	hdrFramebuffer.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	hdrFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	hdrFramebuffer.SetAntialiasing(Antialiasing);

	//cubemapDepthFramebuffer.SetFormat(0);
	cubemapFramebuffer.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	cubemapFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
}

Direct3D11Renderer::~Direct3D11Renderer()
{
	del(simulOpticsRenderer,memoryInterface);
	del(simulWeatherRenderer,memoryInterface);
	del(simulHDRRenderer,memoryInterface);
	del(simulTerrainRenderer,memoryInterface);
}

// D3D11CallbackInterface
bool Direct3D11Renderer::IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,DXGI_FORMAT BackBufferFormat,bool bWindowed)
{
	bool ok=false;
	if(DeviceInfo->DeviceType==D3D_DRIVER_TYPE_HARDWARE&&
		DeviceInfo->MaxLevel>=D3D_FEATURE_LEVEL_11_0)
		ok=true;
	return ok;
}

bool Direct3D11Renderer::ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings)
{
	//pDeviceSettings->d3d11.CreateFlags|=D3D11_CREATE_DEVICE_DEBUG;
	if(pDeviceSettings->d3d11.DriverType!=D3D_DRIVER_TYPE_HARDWARE||pDeviceSettings->MinimumFeatureLevel<D3D_FEATURE_LEVEL_11_0)
		enabled=false;
    return true;
}

HRESULT	Direct3D11Renderer::OnD3D11CreateDevice(ID3D11Device* pd3dDevice,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	m_pd3dDevice=pd3dDevice;
	enabled=true;
	//Set a global device pointer for use by various classes.
	Profiler::GetGlobalProfiler().Initialize(pd3dDevice);
	simul::dx11::UtilityRenderer::RestoreDeviceObjects(pd3dDevice);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	hdrFramebuffer.RestoreDeviceObjects(pd3dDevice);
	//cubemapDepthFramebuffer.SetWidthAndHeight(64,64);
	//cubemapDepthFramebuffer.RestoreDeviceObjects(pd3dDevice);
	cubemapFramebuffer.SetWidthAndHeight(64,64);
	cubemapFramebuffer.RestoreDeviceObjects(pd3dDevice);
	RecompileShaders();
	return S_OK;
}

HRESULT	Direct3D11Renderer::OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice,IDXGISwapChain* pSwapChain
	,const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	if(!enabled)
		return S_OK;
	try
	{
		ScreenWidth=pBackBufferSurfaceDesc->Width;
		ScreenHeight=pBackBufferSurfaceDesc->Height;
		ScreenWidth=pBackBufferSurfaceDesc->Width;
		ScreenHeight=pBackBufferSurfaceDesc->Height;

		int w=ScreenWidth,h=ScreenHeight;
		if(simulWeatherRenderer)
		{
			simulWeatherRenderer->SetScreenSize(ScreenWidth,ScreenHeight);
			int s=simulWeatherRenderer->GetDownscale();
			w=ScreenWidth/s;
			h=ScreenHeight/s;
		}
		if(simulHDRRenderer)
			simulHDRRenderer->SetBufferSize(ScreenWidth,ScreenHeight);
		hdrFramebuffer.SetWidthAndHeight(ScreenWidth,ScreenHeight);
		
		lowResDepthTexture.ensureTexture2DSizeAndFormat(pd3dDevice,w,h,DXGI_FORMAT_R32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
		
		return S_OK;
	}
	catch(...)
	{
		return S_FALSE;
	}
}

void Direct3D11Renderer::RenderCubemap(ID3D11DeviceContext* pContext,D3DXVECTOR3 cam_pos)
{
	D3DXMATRIX view;
	D3DXMATRIX proj;
	D3DXMATRIX view_matrices[6];
	MakeCubeMatrices(view_matrices,cam_pos,ReverseDepth);
	for(int i=0;i<6;i++)
	{
		cubemapFramebuffer.SetCurrentFace(i);
		cubemapFramebuffer.Activate(pContext);
		D3DXMATRIX cube_proj;
		float nearPlane=1.f;
		float farPlane=200000.f;
		static bool r=false;
		if(ReverseDepth)
			cube_proj=simul::camera::Camera::MakeDepthReversedProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane,r);
		else
			cube_proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane,r);
		//cubemapDepthFramebuffer.Activate(pContext);
		//cubemapDepthFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
		if(simulTerrainRenderer)
		{
			simulTerrainRenderer->SetMatrices(view_matrices[i],cube_proj);
		//	simulTerrainRenderer->Render(pContext,1.f);
		}
		cubemapFramebuffer.DeactivateDepth(pContext);
		if(simulWeatherRenderer)
		{
			simulWeatherRenderer->SetMatrices(view_matrices[i],cube_proj);
			simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
			simulWeatherRenderer->RenderSkyAsOverlay(pContext,Exposure,false,true,cubemapFramebuffer.GetDepthTex(),NULL,1,relativeViewportTextureRegionXYWH,true);
		}
		cubemapFramebuffer.Deactivate(pContext);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetCubemapTexture(cubemapFramebuffer.GetColorTex());
}

void Direct3D11Renderer::DownscaleDepth(ID3D11DeviceContext* pContext)
{
	ID3DX11EffectTechnique *downscaleComputeTechnique	=mixedResolutionEffect->GetTechniqueByName("downscale_depth");
	HRESULT hr;
	static const int BLOCKWIDTH			=8;
	uint2 subgrid						=uint2((lowResDepthTexture.width+BLOCKWIDTH-1)/BLOCKWIDTH,(lowResDepthTexture.length+BLOCKWIDTH-1)/BLOCKWIDTH);
	
	simul::dx11::setParameter			(mixedResolutionEffect,"source2DTexture",(ID3D11ShaderResourceView*)hdrFramebuffer.GetDepthTex());
	simul::dx11::setUnorderedAccessView	(mixedResolutionEffect,"target2DTexture",lowResDepthTexture.unorderedAccessView);
	
	V_CHECK(ApplyPass(pContext,downscaleComputeTechnique->GetPassByIndex(0)));
	pContext->Dispatch(subgrid.x,subgrid.y,1);
	unbindTextures(mixedResolutionEffect);
	V_CHECK(ApplyPass(pContext,downscaleComputeTechnique->GetPassByIndex(0)));
}

void Direct3D11Renderer::OnD3D11FrameRender(ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dImmediateContext,double fTime, float fTimeStep)
{
	if(!enabled)
		return;
	static int viewport_id=0;
	D3DXMATRIX world,view,proj;
	static float nearPlane=1.f;
	static float farPlane=250000.f;
	if(camera)
	{
		if(ReverseDepth)
			proj=camera->MakeDepthReversedProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight);
		else
			proj=camera->MakeProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight,false);
		view=camera->MakeViewMatrix();
		D3DXMatrixIdentity(&world);
	}
	simul::dx11::UtilityRenderer::SetMatrices(view,proj);
	D3DXMatrixIdentity(&world);
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetMatrices(view,proj);
		simulWeatherRenderer->PreRenderUpdate(pd3dImmediateContext);
	}
	if(MakeCubemap)
	{
		D3DXVECTOR3 cam_pos=simul::dx11::GetCameraPosVector(view);
		RenderCubemap(pd3dImmediateContext,cam_pos);
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		hdrFramebuffer.SetAntialiasing(Antialiasing);
		hdrFramebuffer.Activate(pd3dImmediateContext);
		hdrFramebuffer.Clear(pd3dImmediateContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
	}
	else
	{
		hdrFramebuffer.ActivateDepth(pd3dImmediateContext);
		hdrFramebuffer.Clear(pd3dImmediateContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetMatrices(view,proj);
	
	if(simulTerrainRenderer&&ShowTerrain)
	{
		simulTerrainRenderer->SetMatrices(view,proj);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture());
		simulTerrainRenderer->Render(pd3dImmediateContext,1.f);	
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->RenderCelestialBackground(pd3dImmediateContext,Exposure);
	hdrFramebuffer.DeactivateDepth(pd3dImmediateContext);
	void *depthTexture=hdrFramebuffer.GetDepthTex();
	if(simulWeatherRenderer)
	{
		DownscaleDepth(pd3dImmediateContext);
		simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
		const void* skyBufferDepthTex = UseSkyBuffer? lowResDepthTexture.shaderResourceView : depthTexture;
		simulWeatherRenderer->RenderSkyAsOverlay(pd3dImmediateContext,Exposure,UseSkyBuffer,false,depthTexture,skyBufferDepthTex,viewport_id,relativeViewportTextureRegionXYWH,true);

		simulWeatherRenderer->RenderLightning(pd3dImmediateContext,viewport_id);
		simulWeatherRenderer->DoOcclusionTests();

		simulWeatherRenderer->RenderPrecipitation(pd3dImmediateContext);
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light;
			if(simulWeatherRenderer->GetSkyRenderer())
			{
				dir=simulWeatherRenderer->GetEnvironment()->skyKeyframer->GetDirectionToSun();
				light=simulWeatherRenderer->GetSkyRenderer()->GetLightColour();
				simulOpticsRenderer->SetMatrices(view,proj);
				float occ=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
				float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
				simulOpticsRenderer->RenderFlare(pd3dImmediateContext,exp,dir,light);
			}
		}
		if(ShowCubemaps&&cubemapFramebuffer.IsValid())
			UtilityRenderer::DrawCubemap(pd3dImmediateContext,(ID3D1xShaderResourceView*)cubemapFramebuffer.GetColorTex(),view,proj);

		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			hdrFramebuffer.Deactivate(pd3dImmediateContext);
			simulHDRRenderer->Render(pd3dImmediateContext,hdrFramebuffer.GetColorTex());
		}
		if(simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(pd3dImmediateContext,ScreenWidth,ScreenHeight);
		if(ShowFades&&simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(pd3dImmediateContext,ScreenWidth,ScreenHeight);

		
	simul::dx11::UtilityRenderer::SetScreenSize(ScreenWidth,ScreenHeight);
	simul::dx11::setParameter(simul::dx11::UtilityRenderer::m_pDebugEffect,"imageTexture",lowResDepthTexture.shaderResourceView);
	UtilityRenderer::DrawQuad2(pd3dImmediateContext,0,0,lowResDepthTexture.width,lowResDepthTexture.length,simul::dx11::UtilityRenderer::m_pDebugEffect,simul::dx11::UtilityRenderer::m_pDebugEffect->GetTechniqueByName("textured"));

		if(ShowCloudCrossSections&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(pd3dImmediateContext,ScreenWidth,ScreenHeight);
			//..simulWeatherRenderer->GetCloudRenderer()->RenderDistances(width,height);
			simulWeatherRenderer->GetCloudRenderer()->RenderAuxiliaryTextures(pd3dImmediateContext,ScreenWidth,ScreenHeight);
			simulWeatherRenderer->RenderFramebufferDepth(pd3dImmediateContext,ScreenWidth,ScreenHeight);
		}
		if(Show2DCloudTextures&&simulWeatherRenderer->Get2DCloudRenderer())
		{
			simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(pd3dImmediateContext,ScreenWidth,ScreenHeight);
			
		}
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(pd3dImmediateContext,ScreenWidth,ScreenHeight);
		}
	}
}

void Direct3D11Renderer::SaveScreenshot(const char *filename_utf8)
{
	screenshotFilenameUtf8=filename_utf8;
	simul::dx11::Framebuffer fb(ScreenWidth,ScreenHeight);
	fb.RestoreDeviceObjects(m_pd3dDevice);
	ID3D11DeviceContext*			pImmediateContext;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	fb.Activate(pImmediateContext);
	OnD3D11FrameRender(m_pd3dDevice,pImmediateContext,0.f,0.f);
	fb.Deactivate(pImmediateContext);
	simul::dx11::SaveTexture(m_pd3dDevice,(ID3D11Texture2D *)(fb.GetColorTexture()),screenshotFilenameUtf8.c_str());
	SAFE_RELEASE(pImmediateContext);
}

void Direct3D11Renderer::OnD3D11LostDevice()
{
	std::cout<<"Direct3D11Renderer::OnD3D11LostDevice"<<std::endl;
	Profiler::GetGlobalProfiler().Uninitialize();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->InvalidateDeviceObjects();
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	hdrFramebuffer.InvalidateDeviceObjects();
	lowResDepthTexture.release();
	//cubemapDepthFramebuffer.InvalidateDeviceObjects();
	cubemapFramebuffer.InvalidateDeviceObjects();
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	SAFE_RELEASE(mixedResolutionEffect);
}

void Direct3D11Renderer::OnD3D11DestroyDevice()
{
	OnD3D11LostDevice();
	// We don't clear the renderers because InvalidateDeviceObjects has already handled DX-specific destruction
	// And after OnD3D11DestroyDevice we might go back to startup without destroying the renderer.
}

void Direct3D11Renderer::OnD3D11ReleasingSwapChain()
{
    //Profiler::GetGlobalProfiler().Uninitialize();
	//if(simulWeatherRenderer)
	//	simulWeatherRenderer->InvalidateDeviceObjects();
	//if(simulHDRRenderer)
	//	simulHDRRenderer->InvalidateDeviceObjects();
	//OnD3D11LostDevice();
}

bool Direct3D11Renderer::OnDeviceRemoved()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	OnD3D11LostDevice();
	return true;
}

void Direct3D11Renderer::RecompileShaders()
{
	simul::dx11::UtilityRenderer::RecompileShaders();
	if(simulWeatherRenderer)
		simulWeatherRenderer->RecompileShaders();
	if(simulOpticsRenderer)
		simulOpticsRenderer->RecompileShaders();
	if(simulTerrainRenderer)
		simulTerrainRenderer->RecompileShaders();
	if(simulHDRRenderer)
		simulHDRRenderer->RecompileShaders();
//	if(simulTerrainRenderer.get())
//		simulTerrainRenderer->RecompileShaders();
	SAFE_RELEASE(mixedResolutionEffect);
	HRESULT hr=CreateEffect(m_pd3dDevice,&mixedResolutionEffect,"mixed_resolution.fx");
}

void    Direct3D11Renderer::OnFrameMove(double fTime,float fTimeStep)
{
}

const char *Direct3D11Renderer::GetDebugText() const
{
	if(!ShowOSD)
		return " ";
	static std::string str;
	str="DirectX 11\n";
	//if(simulWeatherRenderer)
//		str+=simulWeatherRenderer->GetDebugText();
	str+=Profiler::GetGlobalProfiler().GetDebugText();
	return str.c_str();
}

void Direct3D11Renderer::ReverseDepthChanged()
{
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulHDRRenderer)
		simulHDRRenderer->SetReverseDepth(ReverseDepth);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetReverseDepth(ReverseDepth);
}
