#include "Simul/Platform/DirectX12/Direct3D12Manager.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/DirectX12/MacrosDx1x.h"
#include "Simul/Platform/DirectX12/Utilities.h"
#include "Simul/Platform/DirectX12/SimulDirectXHeader.h"

#include <iomanip>

using namespace simul;
using namespace dx12;

Window::Window():
	hwnd(0),
	renderer(nullptr),
	view_id(-1),
	vsync(false),
	SwapChain(nullptr),
	FrameIndex(0)
{
	for (UINT f = 0; f < FrameCount; f++)
	{
		FenceValues[f] = 0;
	}
}

Window::~Window()
{
	Release();
}

void Window::RestoreDeviceObjects(ID3D12Device* d3dDevice, bool m_vsync_enabled, int numerator, int denominator)
{
	if (!d3dDevice)
		return;

	HRESULT res = S_FALSE;
	RECT rect	= {};

#if defined(WINVER) && !defined(_XBOX_ONE)
	GetWindowRect(hwnd, &rect);
#endif

	int screenWidth			= abs(rect.right - rect.left);
	int screenHeight		= abs(rect.bottom - rect.top);

	// Viewport
	Viewport.TopLeftX		= 0;
	Viewport.TopLeftY		= 0;
	Viewport.Width		= (float)screenWidth;
	Viewport.Height		= (float)screenHeight;
	Viewport.MinDepth		= 0.0f;
	Viewport.MaxDepth		= 1.0f;

	// Scissor
	Scissor.left		= 0;
	Scissor.top		= 0;
	Scissor.right		= screenWidth;
	Scissor.bottom	= screenHeight;

#ifndef _XBOX_ONE
	// Dx12 swap chain	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc12	= {};
	swapChainDesc12.BufferCount				= FrameCount;
	swapChainDesc12.Width					= screenWidth; 
	swapChainDesc12.Height					= screenHeight; 
	swapChainDesc12.Format					= DXGI_FORMAT_R16G16B16A16_FLOAT;
	swapChainDesc12.BufferUsage				= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc12.SwapEffect				= DXGI_SWAP_EFFECT_FLIP_DISCARD; 
	swapChainDesc12.SampleDesc.Count		= 1;
	swapChainDesc12.SampleDesc.Quality		= 0;

	IDXGIFactory4* factory		= nullptr;
	res							= CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
	SIMUL_ASSERT(res == S_OK);

	// Create a command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type	= D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	res				= d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&CommandQueue));
	SIMUL_ASSERT(res == S_OK);

	// Create it
	IDXGISwapChain1* swapChain	= nullptr;
	res							= factory->CreateSwapChainForHwnd
	(
		CommandQueue,
		hwnd,
		&swapChainDesc12,
		nullptr,
		nullptr,
		&swapChain
	);
	SIMUL_ASSERT(res == S_OK);

	// Assign and query
	factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	swapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void **)&SwapChain);
	FrameIndex = SwapChain->GetCurrentBackBufferIndex();
#endif

	// Initialize colour and depth surfaces
	CreateRenderTarget(d3dDevice);		
	CreateDepthStencil(d3dDevice);

	// Create the command list
#ifdef _XBOX_ONE
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocators[FrameIndex], nullptr, IID_GRAPHICS_PPV_ARGS(&CommandList));
#else
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocators[FrameIndex], nullptr, IID_PPV_ARGS(&CommandList));
#endif
	CommandList->SetName(L"WindowCommandList");

#ifndef _XBOX_ONE
	SAFE_RELEASE(factory);
	SAFE_RELEASE(swapChain);
#endif
}


void Window::ResizeSwapChain(ID3D12Device* d3dDevice)
{
	// https://github.com/Microsoft/DirectX-Graphics-Samples/issues/48
	// TO-DO: resize doesnt work yet
	// TO-DO: Update the viewports and the depth texture

	// We should have a wait here so we can delete the safelly the surfaces
	
	RECT rect;
#if defined(WINVER) &&!defined(_XBOX_ONE)
	if(!GetWindowRect(hwnd,&rect))
		return;
#endif
	int W	=abs(rect.right-rect.left);
	int H	=abs(rect.bottom-rect.top);

	// DX12 viewport
	Viewport.TopLeftX		= 0;
	Viewport.TopLeftY		= 0;
	Viewport.Width		= (float)W;
	Viewport.Height		= (float)H;
	Viewport.MinDepth		= 0.0f;
	Viewport.MaxDepth		= 1.0f;

	// DX12 scissor rect	
	Scissor.left		= 0;
	Scissor.top		= 0;
	Scissor.right		= W;
	Scissor.bottom	= H;

	DXGI_SWAP_CHAIN_DESC1 swapDesc;
	HRESULT hr = SwapChain->GetDesc1(&swapDesc);
	if (hr != S_OK)
		return;
	if (swapDesc.Width == W && swapDesc.Height == H)
		return;

	// Nacho: that format ?? We should make a member to store this format
	hr = SwapChain->ResizeBuffers(2,W,H,DXGI_FORMAT_R8G8B8A8_UNORM,0);	

	CreateRenderTarget(d3dDevice);
	CreateDepthStencil(d3dDevice);

	DXGI_SURFACE_DESC surfaceDesc;
	SwapChain->GetDesc1(&swapDesc);
	surfaceDesc.Format		=swapDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.Width;
	surfaceDesc.Height		=swapDesc.Height;

	if(renderer)
		renderer->ResizeView(view_id,surfaceDesc.Width,surfaceDesc.Height);
}

void Window::CreateRenderTarget(ID3D12Device* d3dDevice)
{
	HRESULT result = S_OK;
	if(!d3dDevice || !SwapChain)
		return;

	// Describe and create a render target view (RTV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc	= {};
	rtvHeapDesc.NumDescriptors				= FrameCount; 
	rtvHeapDesc.Type						= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
#ifdef _XBOX_ONE
	result									= d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_GRAPHICS_PPV_ARGS(&RTHeap));
#else
	result									= d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&RTHeap));
#endif
	SIMUL_ASSERT(result == S_OK);

	RTDescriptorSize						= d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	RTHeap->SetName(L"WindowRTHeap");
	
	// Back buffers creation:
	// 1) Get the back buffer resources from the swap chain
	// 2) Create DX12 descriptor for each back buffer
	// 3) Create command allocators
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(RTHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT n = 0; n < FrameCount; n++)
	{
		// Store a DX12 Cpu handle for the render targets
		RTHandles[n] = rtvHandle;

		// 1)
		result = SwapChain->GetBuffer(n, __uuidof(ID3D12Resource), (LPVOID*)&BackBuffers[n]);
		SIMUL_ASSERT(result == S_OK);

		// 2)
		d3dDevice->CreateRenderTargetView(BackBuffers[n], nullptr, rtvHandle);
		BackBuffers[n]->SetName(L"WindowBackBuffer");
		rtvHandle.Offset(1, RTDescriptorSize);

		// 3)
#ifdef _XBOX_ONE
		result = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_GRAPHICS_PPV_ARGS(&CommandAllocators[n]));
#else
		result = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocators[n]));
#endif
		SIMUL_ASSERT(result == S_OK);
	}
}

void Window::CreateDepthStencil(ID3D12Device * d3dDevice)
{
	HRESULT res = S_FALSE;
	if (!d3dDevice)
		return;

	// Create Depth Stencil desriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC dsHeapDesc	= {};
	dsHeapDesc.NumDescriptors				= 1;
	dsHeapDesc.Type							= D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsHeapDesc.Flags						= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	res = d3dDevice->CreateDescriptorHeap
	(
		&dsHeapDesc,
#ifdef _XBOX_ONE
		IID_GRAPHICS_PPV_ARGS(&DSHeap)
#else
		IID_PPV_ARGS(&DSHeap)
#endif
	);
	SIMUL_ASSERT(res == S_OK);

	// Default clear
	D3D12_CLEAR_VALUE depthClear	= {};
	depthClear.Format				= DepthStencilFormat;
	depthClear.DepthStencil.Depth	= 0.0f;
	depthClear.DepthStencil.Stencil = 0;

	// Create the depth stencil texture resource
	res = d3dDevice->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DepthStencilFormat, (UINT64)Viewport.Width, (UINT64)Viewport.Height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClear,
#ifdef _XBOX_ONE
		IID_GRAPHICS_PPV_ARGS(&DepthStencilBuffer)
#else
		IID_PPV_ARGS(&DepthStencilBuffer)
#endif
	);
	SIMUL_ASSERT(res == S_OK);
	DepthStencilBuffer->SetName(L"WindowDepthStencilBuffer");

	// Create a descriptor of this ds
	D3D12_DEPTH_STENCIL_VIEW_DESC dsViewDesc	= {};
	dsViewDesc.Format							= DepthStencilFormat;
	dsViewDesc.ViewDimension					= D3D12_DSV_DIMENSION_TEXTURE2D;
	dsViewDesc.Flags							= D3D12_DSV_FLAG_NONE;
	d3dDevice->CreateDepthStencilView(DepthStencilBuffer, &dsViewDesc, DSHeap->GetCPUDescriptorHandleForHeapStart());
}

void Window::SetRenderer(crossplatform::PlatformRendererInterface *ci,int vw_id)
{
	if(renderer==ci)
		return;
	if(renderer)
		renderer->RemoveView(view_id);
	view_id		=vw_id;
	renderer	=ci;
	if(!SwapChain)
		return;

	DXGI_SWAP_CHAIN_DESC	swapDesc;
	DXGI_SURFACE_DESC		surfaceDesc;
	SwapChain->GetDesc(&swapDesc);
	surfaceDesc.Format		=swapDesc.BufferDesc.Format;
	surfaceDesc.SampleDesc	=swapDesc.SampleDesc;
	surfaceDesc.Width		=swapDesc.BufferDesc.Width;
	surfaceDesc.Height		=swapDesc.BufferDesc.Height;
	if(view_id<0)
		view_id				=renderer->AddView();
	renderer->ResizeView(view_id,surfaceDesc.Width,surfaceDesc.Height);
}

void Window::Release()
{
	// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
	if(SwapChain)
		SwapChain->SetFullscreenState(false, NULL);

	SAFE_RELEASE(SwapChain);
	SAFE_RELEASE_ARRAY(BackBuffers,FrameCount);
	SAFE_RELEASE(RTHeap);
	SAFE_RELEASE(DepthStencilBuffer);
	SAFE_RELEASE(DSHeap);
	SAFE_RELEASE_ARRAY(CommandAllocators,FrameCount);
}

Direct3D12Manager::Direct3D12Manager():
	mDevice(nullptr)
{
}

Direct3D12Manager::~Direct3D12Manager()
{
	Shutdown();
}

void Direct3D12Manager::Initialize(bool use_debug,bool instrument, bool default_driver )
{
	HRESULT res			= S_FALSE;

	// Store the vsync setting.
	m_vsync_enabled			= false;

#ifndef _XBOX_ONE

	// Debug layer
	UINT dxgiFactoryFlags = 0;
	if (use_debug)
	{
		ID3D12Debug* debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			debugController->EnableDebugLayer();

			// Enable GPU validation (it will report a list of errors if ocurred after ExecuteCommandList())
			bool doGPUValidation = false;
			if (doGPUValidation)
			{
				ID3D12Debug1* debugController1 = nullptr;
				debugController->QueryInterface(IID_PPV_ARGS(&debugController1));
				debugController1->SetEnableGPUBasedValidation(true);
			}
		}
		SAFE_RELEASE(debugController);
	}

	// Create a DirectX graphics interface factory.
	IDXGIFactory4* factory	= nullptr;
	res						= CreateDXGIFactory2(dxgiFactoryFlags,IID_PPV_ARGS(&factory));
	SIMUL_ASSERT(res == S_OK);

	bool mUseWarpDevice = false;
	// Create device with the warp adapter
	if (mUseWarpDevice)
	{
		IDXGIAdapter* warpAdapter	= nullptr;
		factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
		res							= D3D12CreateDevice(warpAdapter,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&mDevice));
		SIMUL_ASSERT(res == S_OK);
		SAFE_RELEASE(warpAdapter);
	}
	// Create device with hardware adapter
	else
	{
		IDXGIAdapter1* hardwareAdapter			= nullptr;
		DXGI_ADAPTER_DESC1 hardwareAdapterDesc	= {};
		int curAdapterIdx						= 0;
		bool adapterFound						= false;

		while (factory->EnumAdapters1(curAdapterIdx, &hardwareAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			// Query description
			hardwareAdapter->GetDesc1(&hardwareAdapterDesc);

			// Ignore warp adapter
			if (hardwareAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				curAdapterIdx++;
				continue;
			}

			// Check if has the right feature level
			res = D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(res))
			{
				adapterFound = true;
				break;
			}
			curAdapterIdx++;
		}
		res = D3D12CreateDevice(hardwareAdapter,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&mDevice));
		SIMUL_ASSERT(res == S_OK);

		// Set break on_x settings
		bool breakOnWarning = false;
		if (breakOnWarning)
		{
			ID3D12InfoQueue* infoQueue = nullptr;
			mDevice->QueryInterface(IID_PPV_ARGS(&infoQueue));
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		}

		// Store information about the GPU
		m_videoCardMemory = (int)(hardwareAdapterDesc.DedicatedVideoMemory / 1024 / 1024);
		size_t stringLength;
		wcstombs_s(&stringLength, m_videoCardDescription, 128, hardwareAdapterDesc.Description, 128);

		// Log
		SIMUL_COUT << "Adapter: " << m_videoCardDescription << std::endl;
		SIMUL_COUT << "Adapter memory: " << m_videoCardMemory << "(MB)" << std::endl;

		// Enumerate outputs(monitors)
		IDXGIOutput* output = nullptr;
		int outputIdx		= 0;
		while (hardwareAdapter->EnumOutputs(outputIdx, &output) != DXGI_ERROR_NOT_FOUND)
		{
			mOutputs[outputIdx] = output;
			SIMUL_ASSERT(res == S_OK);
			outputIdx++;
			if (outputIdx>100)
			{
				std::cerr << "Tried 100 outputs: no adaptor was found." << std::endl;
				return;
			}
		}
		SAFE_RELEASE(hardwareAdapter);
	}
	SAFE_RELEASE(factory);
#endif
}

int Direct3D12Manager::GetNumOutputs()
{
	return (int)mOutputs.size();
}

crossplatform::Output Direct3D12Manager::GetOutput(int i)
{
	unsigned numModes;
	crossplatform::Output o;
	IDXGIOutput *output=mOutputs[i];
#ifndef _XBOX_ONE
	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	HRESULT result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	SIMUL_ASSERT(result==S_OK);

	DXGI_OUTPUT_DESC outputDesc;
	output->GetDesc(&outputDesc);
	o.width		=abs(outputDesc.DesktopCoordinates.right-outputDesc.DesktopCoordinates.left);
	o.height	=abs(outputDesc.DesktopCoordinates.top-outputDesc.DesktopCoordinates.bottom);

	// Now get extended information about what monitor this is:
	
#if defined(WINVER) &&!defined(_XBOX_ONE)
	MONITORINFOEX monitor;
    monitor.cbSize = sizeof(monitor);
    if (::GetMonitorInfo(outputDesc.Monitor, &monitor) && monitor.szDevice[0])
    {
		DISPLAY_DEVICE dispDev;
        memset(&dispDev, 0, sizeof(dispDev));
		dispDev.cb = sizeof(dispDev);

       if (::EnumDisplayDevices(monitor.szDevice, 0, &dispDev, 0))
       {
           o.monitorName=base::WStringToUtf8(dispDev.DeviceName);
           o.desktopX	=monitor.rcMonitor.left;
           o.desktopY	=monitor.rcMonitor.top;
       }
   }
#endif
	// Create a list to hold all the possible display modes for this monitor/video card combination.
	DXGI_MODE_DESC* displayModeList;
	displayModeList = new DXGI_MODE_DESC[numModes];

	// Now fill the display mode list structures.
	result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	SIMUL_ASSERT(result==S_OK);
	
	// It is useful to get the refresh rate from the video card/monitor.
	//Each computer may be slightly different so we will need to query for that information.
	//We query for the numerator and denominator values and then pass them to DirectX during the setup and it will calculate the proper refresh rate.
	// If we don't do this and just set the refresh rate to a default value which may not exist on all computers then DirectX will respond by performing
	// a blit instead of a buffer flip which will degrade performance and give us annoying errors in the debug output.


	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	for(i=0; i<(int)numModes; i++)
	{
		if(displayModeList[i].Width == (unsigned int)o.width)
		{
			if(displayModeList[i].Height == (unsigned int)o.height)
			{
				o.numerator = displayModeList[i].RefreshRate.Numerator;
				o.denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}
	// Release the display mode list.
	delete [] displayModeList;
	displayModeList = 0;
#endif
	return o;
}
void Direct3D12Manager::Shutdown()
{
	// TO-DO: wait for the GPU to complete last work
	for(OutputMap::iterator i=mOutputs.begin();i!=mOutputs.end();i++)
	{
		SAFE_RELEASE(i->second);
	}
	mOutputs.clear();
	
	for(WindowMap::iterator i=mWindows.begin();i!=mWindows.end();i++)
	{
		SetFullScreen(i->second->hwnd,false,0);
		delete i->second;
	}
	mWindows.clear();

	ReportMessageFilterState();

	SAFE_RELEASE(mDevice);
	//SAFE_RELEASE(CommandQueue);
	//SAFE_RELEASE(CommandList);
	//SAFE_DELETE(m_fence);
}

void Direct3D12Manager::RemoveWindow(HWND hwnd)
{
	if(mWindows.find(hwnd)==mWindows.end())
		return;
	Window *w=mWindows[hwnd];
	SetFullScreen(hwnd,false,0);
	delete w;
	mWindows.erase(hwnd);
}

IDXGISwapChain* Direct3D12Manager::GetSwapChain(HWND h)
{
	if(mWindows.find(h)==mWindows.end())
		return NULL;
	Window *w=mWindows[h];
	if(!w)
		return NULL;
	return w->SwapChain;
}

void Direct3D12Manager::Render(HWND h)
{
	HRESULT res = S_FALSE;

	// Error checking
	if(mWindows.find(h)==mWindows.end())
		return;
	Window *w=mWindows[h];
	if(!w)
	{
		SIMUL_CERR<<"No window exists for HWND "<<std::hex<<h<<std::endl;
		return;
	}
	if(h!=w->hwnd)
	{
		SIMUL_CERR<<"Window for HWND "<<std::hex<<h<<" has hwnd "<<w->hwnd<<std::endl;
		return;
	}

	UINT frameIndex = 0;
#ifdef _XBOX_ONE
	frameIndex = 0;
#else
	frameIndex = w->SwapChain->GetCurrentBackBufferIndex();
#endif

	if(!w->BackBuffers[frameIndex])
	{
		SIMUL_CERR<< "No renderTarget exists for HWND "<<std::hex<<h<<std::endl;
		return;
	}

	// Get the window command list
	auto cmdList = w->CommandList;
	SIMUL_ASSERT(cmdList != nullptr);

	// To start, first we have to setup dx12 for the new frame
	// Reset command allocators	
	res = w->CommandAllocators[frameIndex]->Reset();
	SIMUL_ASSERT(res == S_OK);

	// Reset command list
	res = cmdList->Reset(w->CommandAllocators[frameIndex],nullptr);
	SIMUL_ASSERT(res == S_OK);

	// Set viewport 
	cmdList->RSSetViewports(1, &w->Viewport);
	cmdList->RSSetScissorRects(1, &w->Scissor);

	// Indicate that the back buffer will be used as a render target.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(w->BackBuffers[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	cmdList->OMSetRenderTargets(1, &w->RTHandles[frameIndex], FALSE, &w->DSHeap->GetCPUDescriptorHandleForHeapStart());

	// Submit commands
	const float kClearColor[4] = { 0.0f,0.0f,0.0f,1.0f };
	cmdList->ClearRenderTargetView(w->RTHandles[frameIndex], kClearColor , 0, nullptr);
	cmdList->ClearDepthStencilView(w->DSHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

	// Call the window renderer
	if (w->renderer)
	{
		// Pass both colour and depth descriptors
		void* pParams[2] =
		{
			(void*)&w->RTHandles[frameIndex],
			(void*)&w->DSHeap->GetCPUDescriptorHandleForHeapStart()
		};
		w->renderer->Render(w->view_id, cmdList, pParams,w->Scissor.right,w->Scissor.bottom);
	}

	// Get ready to present
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(w->BackBuffers[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Closing  the command list and executing it with the recorded commands
	res = cmdList->Close();
	SIMUL_ASSERT(res == S_OK);
	ID3D12CommandList* ppCommandLists[] = { cmdList };
	w->CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	static DWORD dwFlags		= 0;
	static UINT SyncInterval	= 1;									
	res = w->SwapChain->Present(SyncInterval,dwFlags);			
	SIMUL_ASSERT(res == S_OK);

	MoveToNextFrame(w);
}

void Direct3D12Manager::SetRenderer(HWND hwnd,crossplatform::PlatformRendererInterface *ci, int view_id)
{
	if(mWindows.find(hwnd)==mWindows.end())
		return;
	Window *w=mWindows[hwnd];
	if(!w)
		return;
	w->SetRenderer(ci,  view_id);
}

int Direct3D12Manager::GetViewId(HWND hwnd)
{
	if(mWindows.find(hwnd)==mWindows.end())
		return -1;
	Window *w=mWindows[hwnd];
	return w->view_id;
}

Window *Direct3D12Manager::GetWindow(HWND hwnd)
{
	if(mWindows.find(hwnd)==mWindows.end())
		return NULL;
	Window *w=mWindows[hwnd];
	return w;
}

void Direct3D12Manager::ReportMessageFilterState()
{

}

void Direct3D12Manager::SetFullScreen(HWND hwnd,bool fullscreen,int which_output)
{
	HRESULT res = S_FALSE;
	Window *w=(Window*)GetWindow(hwnd);
	if(!w)
		return;
	IDXGIOutput *output=mOutputs[which_output];
	if(!w->SwapChain)
		return;
	BOOL current_fullscreen;
	res = w->SwapChain->GetFullscreenState(&current_fullscreen, NULL);
	SIMUL_ASSERT(res == S_OK);
	if((current_fullscreen==TRUE)==fullscreen)
		return;
	res = w->SwapChain->SetFullscreenState(fullscreen, NULL);
	ResizeSwapChain(hwnd);
}

void Direct3D12Manager::ResizeSwapChain(HWND hwnd)
{
	if(mWindows.find(hwnd)==mWindows.end())
		return;
	Window *w=mWindows[hwnd];
	if(!w)
		return;
	w->ResizeSwapChain(mDevice);
}

void Direct3D12Manager::AddWindow(HWND hwnd)
{
	if(mWindows.find(hwnd)!=mWindows.end())
		return;
	Window *window	= new Window;
	mWindows[hwnd]	= window;
	window->hwnd	= hwnd;

	crossplatform::Output o = GetOutput(0);
	window->RestoreDeviceObjects(mDevice, m_vsync_enabled, o.numerator, o.denominator);
}

void simul::dx12::Direct3D12Manager::InitialWaitForGpu(HWND hwnd)
{
	auto w = GetWindow(hwnd);
	SIMUL_ASSERT(w != nullptr);

	// Close the command list and execute it to begin initial copies
	w->CommandList->Close();
	ID3D12CommandList* ppCommandLists[] = { w->CommandList };
	w->CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
#ifdef _XBOX_ONE
		mDevice->CreateFence(w->FenceValues[w->FrameIndex], D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(&w->Fence));
#else
		mDevice->CreateFence(w->FenceValues[w->FrameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&w->Fence));
#endif

		w->FenceValues[w->FrameIndex]++;

		// Create an event handle to use for frame synchronization.
		w->FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (w->FenceEvent == nullptr)
		{
			HRESULT result = HRESULT_FROM_WIN32(GetLastError());
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGpu(w);
	}
}

void Direct3D12Manager::WaitForGpu(Window* w)
{
	// Schedule a Signal command in the queue.
	w->CommandQueue->Signal(w->Fence, w->FenceValues[w->FrameIndex]);

	// Wait until the fence has been processed.
	w->Fence->SetEventOnCompletion(w->FenceValues[w->FrameIndex], w->FenceEvent);
	DWORD ret = WaitForSingleObjectEx(w->FenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	w->FenceValues[w->FrameIndex]++;
}

// Prepare to render the next frame.
void Direct3D12Manager::MoveToNextFrame(Window* w)
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = w->FenceValues[w->FrameIndex];
	w->CommandQueue->Signal(w->Fence, currentFenceValue);

	// Update the frame index.
#ifdef _XBOX_ONE
	w->FrameIndex = -1;
#else
	w->FrameIndex = w->SwapChain->GetCurrentBackBufferIndex();
#endif 

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	DWORD ret = 0x1234F0AD;
	if (w->Fence->GetCompletedValue() < w->FenceValues[w->FrameIndex])
	{
		w->Fence->SetEventOnCompletion(w->FenceValues[w->FrameIndex], w->FenceEvent);
		ret = WaitForSingleObjectEx(w->FenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	w->FenceValues[w->FrameIndex] = currentFenceValue + 1;
}