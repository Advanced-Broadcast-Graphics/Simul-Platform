#include "Platform/CrossPlatform/AccelerationStructure.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
namespace simul
{
	namespace dx12
	{
		class AccelerationStructure: public crossplatform::AccelerationStructure
		{
		protected:
		public:
			AccelerationStructure(crossplatform::RenderPlatform *);
			~AccelerationStructure();
			void RestoreDeviceObjects(crossplatform::Mesh *mesh) override;
			void InvalidateDeviceObjects() override;
			ID3D12Resource* AsD3D12ShaderResource(crossplatform::DeviceContext &deviceContext);
			void RuntimeInit(crossplatform::DeviceContext &deviceContext) override;
		protected:
    // Acceleration structure
			ID3D12Resource *bottomLevelAccelerationStructure=nullptr;
			ID3D12Resource *topLevelAccelerationStructure=nullptr;
			ID3D12Resource* scratchResource=nullptr;
			ID3D12Resource* instanceDescs=nullptr;
			ID3D12Resource* transforms=nullptr;
		};
	}
}