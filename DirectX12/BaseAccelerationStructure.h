#pragma once

#include "Platform/CrossPlatform/BaseAccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"

namespace simul
{
	namespace dx12
	{
		static bool GetID3D12Device5andID3D12GraphicsCommandList4(crossplatform::DeviceContext& deviceContext, ID3D12Device5*& device5, ID3D12GraphicsCommandList4*& commandList4)
		{
			ID3D12Device* device = deviceContext.renderPlatform->AsD3D12Device();
			device->QueryInterface(SIMUL_PPV_ARGS(&device5));
			if (!device5)
				return false;

			ID3D12GraphicsCommandList* commandList = deviceContext.asD3D12Context();
			commandList->QueryInterface(SIMUL_PPV_ARGS(&commandList4));
			if (!commandList4)
				return false;

			if (!deviceContext.renderPlatform->HasRenderingFeatures(crossplatform::RenderingFeatures::Raytracing))
				return false;

			return true;
		}
	}
}