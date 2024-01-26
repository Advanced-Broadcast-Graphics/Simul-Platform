#pragma once
#include "Platform/Core/ReadWriteMutex.h"
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable : 4251)
#endif

namespace platform
{
	namespace crossplatform
	{
		class DisplaySurface;
		//! A class for multiple swap chains (i.e. rendering windows) to share the same device.
		//! With each graphics window it manages (identified by HWND's), WindowManager creates and manages a SwapChain instance.
		class SIMUL_CROSSPLATFORM_EXPORT DisplaySurfaceManager: public crossplatform::DisplaySurfaceManagerInterface
		{
		public:
			DisplaySurfaceManager();
			~DisplaySurfaceManager();
			void Initialize(RenderPlatform *r);
			void Shutdown();
			//! Call from rendering thread.
			void RenderAll(bool clear_list = true) override;
			// Implementing Window Manager, which associates Hwnd's with renderers and view ids:
			//! Add a window. Creates a new Swap Chain.
			void AddWindow(cp_hwnd h, crossplatform::PixelFormat pfm=crossplatform::PixelFormat::UNKNOWN, bool vsync = false) override;
			//! Removes the window and destroys its associated Swap Chain.
			void RemoveWindow(cp_hwnd h) override;
			void Render(cp_hwnd hwnd) override;
			void SetRenderer(crossplatform::RenderDelegatorInterface *ci) override;
			void SetFullScreen(cp_hwnd hwnd, bool fullscreen, int which_output) override;
			void ResizeSwapChain(cp_hwnd hwnd) override;
			int GetViewId(cp_hwnd hwnd) override;

			DisplaySurface *GetWindow(cp_hwnd hwnd);

			platform::core::ReadWriteMutex *delegatorReadWriteMutex;
			void EndFrame(bool clear=true);
			typedef std::function<DisplaySurface*(cp_hwnd)> CreateSurfaceDelegate;
			void SetCreateSurfaceDelegate(CreateSurfaceDelegate d)
			{
				createSurfaceDelegate=d;
			}
		protected:
			CreateSurfaceDelegate createSurfaceDelegate;
	
			static const PixelFormat					kDisplayFormat = BGRA_8_UNORM;
			RenderPlatform*								renderPlatform;
			typedef std::map<cp_hwnd, DisplaySurface*>	DisplaySurfaceMap;
			DisplaySurfaceMap							surfaces;
			RenderDelegatorInterface					*renderDelegater=nullptr;
			std::set<cp_hwnd> toRender;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif