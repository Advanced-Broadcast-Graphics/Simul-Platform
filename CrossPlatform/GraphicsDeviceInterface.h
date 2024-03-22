#ifndef GRAPHICSDEVICEINTERFACE
#define GRAPHICSDEVICEINTERFACE
#include <string>

#if !defined(DOXYGEN) && defined(_MSC_VER)
#include <Windows.h>
#define cp_hwnd HWND
#else
typedef void* cp_hwnd;
#endif

#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/RenderDelegate.h"
#include "Platform/CrossPlatform/Export.h"

namespace platform
{
	namespace crossplatform
	{
		struct Output
		{
			std::string monitorName;
			int desktopX;
			int desktopY;
			int width;
			int height;
			int numerator, denominator;
		};
		struct GPUInfo
		{
			std::string name;
			uint64_t memorySize;
		};
		class RenderDelegatorInterface;
		/// An interface class for managing GPU-accelerated graphics windows.
		/// The derived class 
		class SIMUL_CROSSPLATFORM_EXPORT GraphicsDeviceInterface
		{
		public:
			virtual ~GraphicsDeviceInterface(){}
			virtual void	Initialize(bool use_debug, bool instrument, bool default_driver) = 0;
			virtual void	Shutdown()=0;
			virtual void*	GetDevice()=0;
			virtual void*	GetDeviceContext()=0;
			virtual int		GetNumOutputs()=0;
			virtual Output	GetOutput(int i)=0;
			virtual bool	IsActive() const=0;
			virtual GPUInfo	GetGPUInfo() = 0;
		};
		
		class DisplaySurfaceManagerInterface
		{
		public:
			virtual void	AddWindow(cp_hwnd h,crossplatform::PixelFormat pfm=crossplatform::PixelFormat::UNKNOWN,bool vsync=false)=0;
			virtual void	RemoveWindow(cp_hwnd h)=0;
			virtual void	Render(cp_hwnd h)=0;
			virtual void	SetRenderer(RenderDelegatorInterface *ci)=0;
			virtual void	SetFullScreen(cp_hwnd h,bool fullscreen,int which_output)=0;
			virtual void	ResizeSwapChain(cp_hwnd h)=0;
			virtual int		GetViewId(cp_hwnd h)=0;
			virtual void	RenderAll(bool=true)=0;
		};
	}
}
#endif