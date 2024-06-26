#if defined(_GAMING_XBOX_XBOXONE)
#define PLATFORM_SUPPORT_D3D12_RAYTRACING 0
#endif

#if PLATFORM_SUPPORT_D3D12_RAYTRACING
#include "Platform/DirectX12/SimulDirectXHeader.h"
#include "Platform/DirectX12/BaseAccelerationStructure.h"


bool platform::dx12::GetID3D12Device5andID3D12GraphicsCommandList4(crossplatform::DeviceContext& deviceContext, ID3D12Device5*& device5, ID3D12GraphicsCommandList4*& commandList4)
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
#endif