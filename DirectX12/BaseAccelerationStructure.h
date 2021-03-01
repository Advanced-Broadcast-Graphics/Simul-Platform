#pragma once

#include "Platform/CrossPlatform/BaseAccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

#if defined(_XBOX_ONE)
#define PLATFORM_SUPPORT_D3D12_RAYTRACING 0
#endif

namespace simul
{
	namespace dx12
	{
	#if PLATFORM_SUPPORT_D3D12_RAYTRACING
		static bool GetID3D12Device5andID3D12GraphicsCommandList4(crossplatform::DeviceContext& deviceContext, ID3D12Device5*& device5, ID3D12GraphicsCommandList4*& commandList4);
	#endif
	}
}