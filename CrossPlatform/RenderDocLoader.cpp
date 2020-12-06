#if PLATFORM_LOAD_RENDERDOC
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/EnvironmentVariables.h"
#include "Platform/CrossPlatform/RenderDocLoader.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/External/RenderDoc/Include/renderdoc_app.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem>

using namespace simul;
using namespace crossplatform;

static RENDERDOC_API_1_4_1* s_RenderDocAPI = nullptr;
static HMODULE s_HModuleRenderDoc;
static std::filesystem::path s_RenderDocFullpath;

#define STRINGIFY(a) STRINGIFY2(a)
#define STRINGIFY2(a) #a
#define PLATFORM_SOURCE_DIR_STR STRINGIFY(PLATFORM_SOURCE_DIR)

void RenderDocLoader::Load() 
{
	if (s_HModuleRenderDoc)
		return;

	std::string platform_dir = PLATFORM_SOURCE_DIR_STR;
	s_RenderDocFullpath = platform_dir + "/External/RenderDoc/lib/x64/renderdoc.dll";
	s_HModuleRenderDoc = LoadLibraryA(s_RenderDocFullpath.generic_string().c_str());
	errno=0;
	if (!s_HModuleRenderDoc)
	{
		std::string error_str = "RenderDocLoader was unable to load '" + s_RenderDocFullpath.generic_string() + "'. GetLastError: " + std::to_string(GetLastError());
		SIMUL_CERR_ONCE << error_str.c_str() << "\n";
		return;
	}

	pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(s_HModuleRenderDoc, "RENDERDOC_GetAPI");
	if (!RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_1, (void**)&s_RenderDocAPI))
	{
		SIMUL_CERR_ONCE << "RenderDocLoader was unable to initialise RenderDoc.\n";
		return;
	}
	uint32_t pid= s_RenderDocAPI->LaunchReplayUI(1,nullptr);
	if(!pid)
	{
		SIMUL_CERR_ONCE << "Failed to launch replay ui.\n";
		return;
	}
}

void RenderDocLoader::Unload() 
{
	if (s_HModuleRenderDoc)
	{
		if (!FreeLibrary(s_HModuleRenderDoc))
		{
			std::string error_str = "RenderDocLoader was unable to free '" + s_RenderDocFullpath.generic_string() + "'. GetLastError: " + std::to_string(GetLastError());
			SIMUL_CERR_ONCE << error_str.c_str() << "\n";
		}
		errno = 0;
	}
}
void RenderDocLoader::TriggerMultiFrameCapture(uint32_t num)
{
	if (s_RenderDocAPI)
	{
		s_RenderDocAPI->TriggerMultiFrameCapture(num);
	}
}
void RenderDocLoader::StartCapture(RenderPlatform *renderPlatform,void * windowHandlePtr)
{
	// To start a frame capture, call StartFrameCapture.
	// You can specify NULL, NULL for the device to capture on if you have only one device and
	// either no windows at all or only one window, and it will capture from that device.
	// See the documentation below for a longer explanation
	if (s_RenderDocAPI)
	{
		void* nativeDevicePtr=nullptr;
	switch(renderPlatform->GetType())
	{
	case RenderPlatformType::D3D11:
		nativeDevicePtr=renderPlatform->AsD3D11Device();
		break;
	case RenderPlatformType::D3D12:
		nativeDevicePtr = renderPlatform->AsD3D12Device();
		break;
	case RenderPlatformType::Vulkan:
		nativeDevicePtr = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(renderPlatform->AsVulkanInstance());
		break;
		default:
		break;
	}
		//#define RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(inst) (*((void **)(inst)))
		s_RenderDocAPI->StartFrameCapture(nativeDevicePtr,windowHandlePtr);
	}
}
void RenderDocLoader::FinishCapture()
{

	// Your rendering should happen here

	// stop the capture
	if (s_RenderDocAPI)
	{
		if(!s_RenderDocAPI->EndFrameCapture(NULL, NULL))
		{
			SIMUL_CERR_ONCE << "EndFrameCapture failed\n";
		}
	}
}
#else //Dummy class implementation
#include "Platform/CrossPlatform/RenderDocLoader.h"

using namespace simul;
using namespace crossplatform;

void RenderDocLoader::Load() {}
void RenderDocLoader::Unload() {}
void RenderDocLoader::StartCapture(){}
void RenderDocLoader::FinishCapture(){}
#endif