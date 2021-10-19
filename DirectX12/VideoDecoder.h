#pragma once
#include "Export.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/VideoDecoder.h"
#include "Platform/DirectX12/Texture.h"
#include "SimulDirectXHeader.h"
#include "Platform/DirectX12/CommandListController.h"
#include <unordered_map>

struct ID3D12VideoDevice2;
struct ID3D12VideoDecoder1;
struct ID3D12VideoDecoderHeap1;
struct ID3D12VideoProcessor1;
struct ID3D12VideoDecodeCommandList2;

typedef ID3D12VideoDevice2 ID3D12VideoDeviceType;
typedef ID3D12VideoDecodeCommandList2 ID3D12VideoDecodeCommandListType;

namespace cp = simul::crossplatform;

namespace simul
{
	namespace dx12
	{
		class DecoderTexture : public Texture
		{
		public:
			void ChangeState(ID3D12VideoDecodeCommandListType* commandList, bool write = false);
		};

		//! A class to implement common video encodng/decoding functionality for DirectX 12.
		class SIMUL_DIRECTX12_EXPORT VideoDecoder: public cp::VideoDecoder
		{
		public:
			VideoDecoder();
			~VideoDecoder();
			cp::VideoDecoderResult Shutdown() override;
			static cp::VideoDecoderResult CheckSupport(ID3D12VideoDeviceType* deviceHandle, const cp::VideoDecoderParams& decoderParams);

			//
			// Map of possible input and output VP formats
			// 
			static const std::unordered_map<DXGI_FORMAT, DXGI_COLOR_SPACE_TYPE> VideoFormats;

		protected:
			cp::VideoDecoderResult Init() override;
			cp::VideoDecoderResult DecodeFrame(cp::Texture* outputTexture, const void* buffer, size_t bufferSize, const cp::VideoDecodeArgument* decodeArgs = nullptr, uint32_t decodeArgCount = 0) override;
			void* GetGraphicsContext() const override;
			void* GetDecodeContext() const override;
			cp::VideoBuffer* CreateVideoBuffer() const override;
			cp::Texture* CreateDecoderTexture() const override;

		private:
			void Signal(void* context, cp::Fence* fence) override;
			void WaitOnFence(void* context, cp::Fence* fence) override;
			cp::VideoDecoderResult CreateVideoDevice();
			cp::VideoDecoderResult CreateVideoDecoder();
			cp::VideoDecoderResult CreateCommandObjects();

			//! D3D12 video device for hardware accelerated video encoding/decoding
			ID3D12VideoDeviceType* mVideoDevice;
			ID3D12VideoDecoder1* mDecoder;
			ID3D12VideoDecoderHeap1* mHeap;
			CommandListController mGraphicsCLC;
			CommandListController mDecodeCLC;
		};
	}
}