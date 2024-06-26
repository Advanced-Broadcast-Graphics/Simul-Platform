
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/RenderDelegater.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Math/Pi.h"
using namespace platform;
using namespace crossplatform;

RenderDelegator::RenderDelegator(crossplatform::RenderPlatform *r)
	:last_view_id(0)
	,renderPlatform(r)
{
}

RenderDelegator::~RenderDelegator()
{
	OnLostDevice();
}

void RenderDelegator::SetRenderPlatform(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
}

void RenderDelegator::OnLostDevice()
{
	for(auto d:shutdownDeviceDelegates)
		d();
	renderPlatform = nullptr;
	shutdownDeviceDelegates.clear();
}

int	RenderDelegator::AddView()
{
	return last_view_id++;
}

void RenderDelegator::RemoveView	(int view_id)
{
}

void RenderDelegator::ResizeView(int view_id,int w,int h)
{
}

void RenderDelegator::SetRenderDelegate(int view_id,crossplatform::RenderDelegate d)
{
	renderDelegate[view_id]=d;
}

void RenderDelegator::RegisterShutdownDelegate(crossplatform::ShutdownDeviceDelegate d)
{
	shutdownDeviceDelegates.push_back(d);
}

void RenderDelegator::Render(int view_id,void* context,void* rendertarget,int w,int h, long long f, void* context_allocator)
{
	ERRNO_BREAK
	crossplatform::GraphicsDeviceContext deviceContext;
	viewSize[view_id]					= int2(w,h);
	deviceContext.platform_context		= context;
	deviceContext.platform_context_allocator = context_allocator;
	deviceContext.renderPlatform		= renderPlatform;
	deviceContext.viewStruct.view_id	= view_id;
	static platform::core::Timer timer;
	
	deviceContext.predictedDisplayTimeS	=double(timer.AbsoluteTimeMS())*0.001;
	crossplatform::Viewport vps[1];
	vps[0].x = vps[0].y = 0;
	vps[0].w = w;
	vps[0].h = h;
	renderPlatform->SetViewports(deviceContext, 1, vps);
	int2 vs	= viewSize[view_id];

	std::string pn(renderPlatform->GetName());
	deviceContext.setDefaultRenderTargets
		(
			rendertarget, NULL,
			0, 0, vs.x, vs.y
		);

	platform::crossplatform::SetGpuProfilingInterface(deviceContext,renderPlatform->GetGpuProfiler());
	if (renderDelegate[view_id])
	{
		renderDelegate[view_id](deviceContext);
	}
}
