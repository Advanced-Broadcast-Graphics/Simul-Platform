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
#include "Simul/Clouds/BasePrecipitationRenderer.h"
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
		,ShowDepthBuffers(false)
		,ShowLightVolume(false)
		,CelestialDisplay(false)
		,ShowWater(true)
		,MakeCubemap(true)
		,ShowCubemaps(false)
		,ShowRainTextures(false)
		,ReverseDepth(true)
		,ShowOSD(false)
		,Exposure(1.0f)
		,Antialiasing(1)
		,enabled(false)
		,m_pd3dDevice(NULL)
		,mixedResolutionEffect(NULL)
		,lightProbesEffect(NULL)
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
	resolvedDepth_fb.SetFormat(DXGI_FORMAT_R32_FLOAT);
	cubemapFramebuffer.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	cubemapFramebuffer.SetDepthFormat(DXGI_FORMAT_D32_FLOAT);
	
	envmapFramebuffer.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	envmapFramebuffer.SetDepthFormat(DXGI_FORMAT_UNKNOWN);
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
	pDeviceSettings->d3d11.CreateFlags|=D3D11_CREATE_DEVICE_DEBUG;
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
	mixedResolutionConstants.RestoreDeviceObjects(m_pd3dDevice);
	lightProbeConstants.RestoreDeviceObjects(m_pd3dDevice);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(pd3dDevice);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(pd3dDevice);
	hdrFramebuffer.RestoreDeviceObjects(pd3dDevice);
	resolvedDepth_fb.RestoreDeviceObjects(pd3dDevice);
	//cubemapDepthFramebuffer.SetWidthAndHeight(64,64);
	//cubemapDepthFramebuffer.RestoreDeviceObjects(pd3dDevice);
	cubemapFramebuffer.SetWidthAndHeight(64,64);
	cubemapFramebuffer.RestoreDeviceObjects(pd3dDevice);
	envmapFramebuffer.SetWidthAndHeight(64,64);
	envmapFramebuffer.RestoreDeviceObjects(pd3dDevice);
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
		// To debug antialiasing, divide all these by about 4:
		ScreenWidth	=pBackBufferSurfaceDesc->Width;
		ScreenHeight=pBackBufferSurfaceDesc->Height;
		ScreenWidth	=pBackBufferSurfaceDesc->Width;
		ScreenHeight=pBackBufferSurfaceDesc->Height;
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
	MakeCubeMatrices(view_matrices,cam_pos,false);//ReverseDepth);
	cubemapFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
	for(int i=0;i<6;i++)
	{
		cubemapFramebuffer.SetCurrentFace(i);
		cubemapFramebuffer.Activate(pContext);
		D3DXMATRIX cube_proj;
		float nearPlane=1.f;
		float farPlane=200000.f;
		if(ReverseDepth)
			cube_proj=simul::camera::Camera::MakeDepthReversedProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane,false);
		else
			cube_proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,nearPlane,farPlane,false);
		//
		cube_proj._11*=-1.f;
		//cubemapDepthFramebuffer.Activate(pContext);
		if(simulTerrainRenderer&&ShowTerrain)
		{
			simulTerrainRenderer->SetMatrices(view_matrices[i],cube_proj);
			simulTerrainRenderer->Render(pContext,1.f);
		}
		cubemapFramebuffer.DeactivateDepth(pContext);
		if(simulWeatherRenderer)
		{
			simulWeatherRenderer->SetMatrices((const float *)&(view_matrices[i]),(const float *)&cube_proj);
			simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
			void *d=cubemapFramebuffer.GetDepthTex(i);
			simulWeatherRenderer->RenderSkyAsOverlay(pContext,Exposure,false,true,d,NULL,1,relativeViewportTextureRegionXYWH,true);
		}
		cubemapFramebuffer.Deactivate(pContext);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetCubemapTexture(envmapFramebuffer.GetColorTex());
}
	
void Direct3D11Renderer::RenderEnvmap(ID3D11DeviceContext* pContext)
{
	cubemapFramebuffer.CalcSphericalHarmonics(pContext);
	envmapFramebuffer.Clear(pContext,0.f,1.f,0.f,1.f,0.f);
	D3DXMATRIX invViewProj;
	D3DXMATRIX view_matrices[6];
	float cam_pos[]={0,0,0};
	ID3DX11EffectTechnique *tech=lightProbesEffect->GetTechniqueByName("irradiance_map");
	MakeCubeMatrices(view_matrices,cam_pos,false);
	// For each face, 
	for(int i=0;i<6;i++)
	{
		envmapFramebuffer.SetCurrentFace(i);
		envmapFramebuffer.Activate(pContext);
		D3DXMATRIX cube_proj=simul::camera::Camera::MakeProjectionMatrix(pi/2.f,pi/2.f,1.f,200000.f,false);
		cube_proj._11*=-1.f;
		{
			MakeInvViewProjMatrix(invViewProj,view_matrices[i],cube_proj);
			lightProbeConstants.invViewProj=invViewProj;
			//lightProbeConstants.invViewProj.transpose();
			lightProbeConstants.Apply(pContext);
			simul::dx11::setTexture(lightProbesEffect,"basisBuffer"	,cubemapFramebuffer.GetSphericalHarmonics().shaderResourceView);
			ApplyPass(pContext,tech->GetPassByIndex(0));
			UtilityRenderer::DrawQuad(pContext);
		}
		envmapFramebuffer.Deactivate(pContext);
	}
}

void Direct3D11Renderer::ResolveColour(ID3D11DeviceContext* pContext)
{
	static const int BLOCKWIDTH			=8;
	// Resolve the colour buffer:
	resolvedColourTexture.ensureTexture2DSizeAndFormat(m_pd3dDevice,ScreenWidth,ScreenHeight,DXGI_FORMAT_R32G32B32A32_FLOAT,/*computable=*/true,/*rendertarget=*/false);
	simul::dx11::setTexture				(mixedResolutionEffect			,"sourceTextureMS"	,(ID3D11ShaderResourceView*)hdrFramebuffer.GetColorTex());
	simul::dx11::setUnorderedAccessView	(mixedResolutionEffect			,"target2DTexture"	,resolvedColourTexture.unorderedAccessView);
	simul::dx11::applyPass				(pContext,mixedResolutionEffect	,"resolve");
	uint2 subgrid						=uint2((resolvedColourTexture.width+BLOCKWIDTH-1)/BLOCKWIDTH,(resolvedColourTexture.length+BLOCKWIDTH-1)/BLOCKWIDTH);
	pContext->Dispatch(subgrid.x,subgrid.y,1);
	simul::dx11::setTexture				(mixedResolutionEffect	,"sourceTextureMS"	,NULL);
	simul::dx11::setUnorderedAccessView	(mixedResolutionEffect	,"target2DTexture"	,NULL);
	simul::dx11::applyPass(pContext,mixedResolutionEffect,"resolve");
}

void Direct3D11Renderer::ResolveDepth(ID3D11DeviceContext* pContext,const D3DXMATRIX &proj)
{
	if(!simulWeatherRenderer)
		return;
	int s=simulWeatherRenderer->GetDownscale();
	int w=ScreenWidth/s;
	int h=ScreenHeight/s;
	static const int BLOCKWIDTH			=8;
	
	// DXGI_FORMAT_D32_FLOAT->DXGI_FORMAT_R32_FLOAT
	lowResDepthTexture.ensureTexture2DSizeAndFormat(m_pd3dDevice,w,h,DXGI_FORMAT_R32G32B32A32_FLOAT,/*computable=*/true,/*rendertarget=*/false);

	// Resolve depth first:
	ID3D11Texture2D *depthTexture=hdrFramebuffer.GetDepthTexture();
	// Sadly, ResolveSubresource doesn't work for depth. And compute can't do MS lookups. So we use a framebufferinstead.
	resolvedDepth_fb.Activate(pContext);
		simul::dx11::setTexture			(mixedResolutionEffect,"sourceMSDepthTexture"	,(ID3D11ShaderResourceView*)hdrFramebuffer.GetDepthTex());
		ID3DX11EffectTechnique *tech	=mixedResolutionEffect->GetTechniqueByName("resolve_depth");
		V_CHECK(ApplyPass(pContext,tech->GetPassByIndex(0)));
		simul::dx11::UtilityRenderer::DrawQuad(pContext);
	resolvedDepth_fb.Deactivate(pContext);
	//pContext->ResolveSubresource(resolvedDepthTexture.texture, 0, depthTexture, 0, DXGI_FORMAT_R32_FLOAT);
	mixedResolutionConstants.scale=uint2(s,s);
	mixedResolutionConstants.depthToLinFadeDistParams=simulWeatherRenderer->GetBaseSkyRenderer()->GetDepthToDistanceParameters(proj);
	mixedResolutionConstants.nearZ=0;
	mixedResolutionConstants.farZ=0;
	mixedResolutionConstants.Apply(pContext);
	uint2 subgrid						=uint2((lowResDepthTexture.width+BLOCKWIDTH-1)/BLOCKWIDTH,(lowResDepthTexture.length+BLOCKWIDTH-1)/BLOCKWIDTH);
	simul::dx11::setTexture				(mixedResolutionEffect,"sourceMSDepthTexture"	,(ID3D11ShaderResourceView*)hdrFramebuffer.GetDepthTex());
	simul::dx11::setUnorderedAccessView	(mixedResolutionEffect,"target2DTexture"		,lowResDepthTexture.unorderedAccessView);
	
	simul::dx11::applyPass(pContext,mixedResolutionEffect,"downscale_depth_far_near");
	pContext->Dispatch(subgrid.x,subgrid.y,1);
	unbindTextures(mixedResolutionEffect);
	simul::dx11::applyPass(pContext,mixedResolutionEffect,"downscale_depth_far_near");
}

void Direct3D11Renderer::RenderScene(ID3D11DeviceContext* pContext)
{
	{
		// Must have a whole number of full-res pixels per low-res pixel.
		if(simulWeatherRenderer)
		{
			int s=simulWeatherRenderer->GetDownscale();
			int w			=(ScreenWidth +s-1)/s;
			int h			=(ScreenHeight+s-1)/s;
			ScreenWidth		=w*s;
			ScreenHeight	=h*s;
		}
		if(simulWeatherRenderer)
			simulWeatherRenderer->SetScreenSize(ScreenWidth,ScreenHeight);
		if(simulHDRRenderer)
			simulHDRRenderer->SetBufferSize(ScreenWidth,ScreenHeight);
		hdrFramebuffer.SetWidthAndHeight(ScreenWidth,ScreenHeight);
		resolvedDepth_fb.SetWidthAndHeight(ScreenWidth,ScreenHeight);
	}
	static int viewport_id=0;
	D3DXMATRIX view,proj;
	static float nearPlane=1.f;
	static float farPlane=250000.f;
	if(camera)
	{
		if(ReverseDepth)
			proj=camera->MakeDepthReversedProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight);
		else
			proj=camera->MakeProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight,false);
		view=camera->MakeViewMatrix();
	}
	simul::dx11::UtilityRenderer::SetMatrices(view,proj);
	if(simulWeatherRenderer)
	{
		simulWeatherRenderer->SetMatrices((const float *)&view,(const float*)&proj);
		simulWeatherRenderer->PreRenderUpdate(pContext);
	}
	if(MakeCubemap)
	{
		D3DXVECTOR3 cam_pos=simul::dx11::GetCameraPosVector((const float *)&view);
		RenderCubemap(pContext,cam_pos);
		RenderEnvmap(pContext);
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		if(hdrFramebuffer.numAntialiasingSamples!=Antialiasing)
		{
			hdrFramebuffer.SetAntialiasing(Antialiasing);
		}
		hdrFramebuffer.Activate(pContext);
			// Did we fail to set the requested value?
		if(hdrFramebuffer.numAntialiasingSamples!=Antialiasing)
			Antialiasing=hdrFramebuffer.numAntialiasingSamples;
		hdrFramebuffer.Clear(pContext,0.f,0.f,0.f,0.f,ReverseDepth?0.f:1.f);
	}
	else
	{
		hdrFramebuffer.ActivateDepth(pContext);
		hdrFramebuffer.ClearDepth(pContext,ReverseDepth?0.f:1.f);
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetMatrices((const float *)&view,(const float *)&proj);
	
	if(simulTerrainRenderer&&ShowTerrain)
	{
		simulTerrainRenderer->SetMatrices(view,proj);
		if(simulWeatherRenderer&&simulWeatherRenderer->GetBaseCloudRenderer())
			simulTerrainRenderer->SetCloudShadowTexture(simulWeatherRenderer->GetBaseCloudRenderer()->GetCloudShadowTexture());
		simulTerrainRenderer->Render(pContext,1.f);	
	}
	if(simulWeatherRenderer)
		simulWeatherRenderer->RenderCelestialBackground(pContext,Exposure);
	if(simulHDRRenderer&&UseHdrPostprocessor)
		hdrFramebuffer.DeactivateDepth(pContext);
	else
		hdrFramebuffer.Deactivate(pContext);
	if(simulWeatherRenderer)
	{
		ResolveDepth(pContext,proj);
		void *depthTexture=(ID3D11ShaderResourceView*)hdrFramebuffer.GetDepthTex();//resolvedDepth_fb.GetColorTex();
		simul::sky::float4 relativeViewportTextureRegionXYWH(0.0f,0.0f,1.0f,1.0f);
		static bool test=true;
		const void* skyBufferDepthTex = (UseSkyBuffer&test)? lowResDepthTexture.shaderResourceView : depthTexture;
		simulWeatherRenderer->RenderSkyAsOverlay(pContext,Exposure,UseSkyBuffer,false,depthTexture,skyBufferDepthTex,viewport_id,relativeViewportTextureRegionXYWH,true);

		simulWeatherRenderer->RenderLightning(pContext,viewport_id);
		simulWeatherRenderer->DoOcclusionTests();

		simulWeatherRenderer->RenderPrecipitation(pContext);
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
				simulOpticsRenderer->RenderFlare(pContext,exp,dir,light);
			}
		}
	}
	if(simulHDRRenderer&&UseHdrPostprocessor)
	{
		hdrFramebuffer.Deactivate(pContext);
		ResolveColour(pContext);
		simulHDRRenderer->Render(pContext,resolvedColourTexture.shaderResourceView);//hdrFramebuffer.GetColorTex());//
	}
	if(MakeCubemap&&ShowCubemaps&&cubemapFramebuffer.IsValid())
	{
		UtilityRenderer::DrawCubemap(pContext,(ID3D1xShaderResourceView*)cubemapFramebuffer.GetColorTex(),view,proj,-.7f,.7f);
		UtilityRenderer::DrawCubemap(pContext,(ID3D1xShaderResourceView*)envmapFramebuffer.GetColorTex(),view,proj,-.4f,.7f);
	}
}

void Direct3D11Renderer::OnD3D11FrameRender(ID3D11Device* pd3dDevice,ID3D11DeviceContext* pContext,double fTime, float fTimeStep)
{
	if(!enabled)
		return;
	RenderScene(pContext);
	if(simulWeatherRenderer)
	{
		if(simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(pContext,ScreenWidth,ScreenHeight);
		if(ShowFades&&simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(pContext,ScreenWidth,ScreenHeight);
		simul::dx11::UtilityRenderer::SetScreenSize(ScreenWidth,ScreenHeight);
		if(ShowDepthBuffers)
		{
			int w=lowResDepthTexture.width*4;
			int l=lowResDepthTexture.length*4;
			if(w>hdrFramebuffer.Width/4)
			{
				l*=hdrFramebuffer.Width/4;
				l/=w;
				w=hdrFramebuffer.Width/4;
			}
			if(l>hdrFramebuffer.Height/4)
			{
				w*=hdrFramebuffer.Height/4;
				w/=l;
				l=hdrFramebuffer.Height/4;
			}
			UtilityRenderer::DrawTextureMS(	pContext,0*w,0,w,l,(ID3D1xShaderResourceView*)hdrFramebuffer.GetDepthTex());
			UtilityRenderer::DrawTexture(	pContext,1*w,0,w,l,(ID3D1xShaderResourceView*)resolvedDepth_fb.GetColorTex());
			UtilityRenderer::DrawTexture(	pContext,2*w,0,w,l,lowResDepthTexture.shaderResourceView);
			UtilityRenderer::DrawTexture(	pContext,3*w,0,w,l,(ID3D1xShaderResourceView*)cubemapFramebuffer.GetDepthTex(0));
		}
		if(ShowCloudCrossSections&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->RenderFramebufferDepth(						pContext,ScreenWidth,ScreenHeight);
			simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(		pContext,ScreenWidth,ScreenHeight);
			simulWeatherRenderer->GetCloudRenderer()->RenderAuxiliaryTextures(	pContext,ScreenWidth,ScreenHeight);
			simulWeatherRenderer->RenderFramebufferDepth(						pContext,ScreenWidth,ScreenHeight);
		}
		if(Show2DCloudTextures&&simulWeatherRenderer->Get2DCloudRenderer())
		{
			simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(pContext,ScreenWidth,ScreenHeight);
		}
		if(simulWeatherRenderer->GetBasePrecipitationRenderer()&&ShowRainTextures)
		{
			simulWeatherRenderer->GetBasePrecipitationRenderer()->RenderTextures(pContext,ScreenWidth,ScreenHeight);
		}
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
		{
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(pContext,ScreenWidth,ScreenHeight);
		}
	}
	Profiler::GetGlobalProfiler().EndFrame(pContext);
}

void Direct3D11Renderer::SaveScreenshot(const char *filename_utf8)
{
	screenshotFilenameUtf8=filename_utf8;
	simul::dx11::Framebuffer fb(ScreenWidth,ScreenHeight);
	fb.RestoreDeviceObjects(m_pd3dDevice);
	ID3D11DeviceContext*			pImmediateContext;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	fb.Activate(pImmediateContext);
	RenderScene(pImmediateContext);
	fb.Deactivate(pImmediateContext);
	simul::dx11::SaveTexture(m_pd3dDevice,(ID3D11Texture2D *)(fb.GetColorTexture()),screenshotFilenameUtf8.c_str());
	SAFE_RELEASE(pImmediateContext);
}

void Direct3D11Renderer::OnD3D11LostDevice()
{
	std::cout<<"Direct3D11Renderer::OnD3D11LostDevice"<<std::endl;
	Profiler::GetGlobalProfiler().Uninitialize();
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	if(simulHDRRenderer)
		simulHDRRenderer->InvalidateDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->InvalidateDeviceObjects();
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	hdrFramebuffer.InvalidateDeviceObjects();
	resolvedDepth_fb.InvalidateDeviceObjects();
	lowResDepthTexture.release();
	cubemapFramebuffer.InvalidateDeviceObjects();
	simul::dx11::UtilityRenderer::InvalidateDeviceObjects();
	SAFE_RELEASE(mixedResolutionEffect);
	mixedResolutionConstants.InvalidateDeviceObjects();
	lightProbeConstants.InvalidateDeviceObjects();
	resolvedColourTexture.release();
	SAFE_RELEASE(lightProbesEffect);
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
	cubemapFramebuffer.RecompileShaders();
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
	V_CHECK(CreateEffect(m_pd3dDevice,&mixedResolutionEffect,"mixed_resolution.fx"));
	mixedResolutionConstants.LinkToEffect(mixedResolutionEffect,"MixedResolutionConstants");
	SAFE_RELEASE(lightProbesEffect);
	V_CHECK(CreateEffect(m_pd3dDevice,&lightProbesEffect,"light_probes.fx"));
	lightProbeConstants.LinkToEffect(lightProbesEffect,"LightProbeConstants");
}

void Direct3D11Renderer::OnFrameMove(double fTime,float fTimeStep)
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
