#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/DeviceContext.h"
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		class Mesh;
		class SIMUL_CROSSPLATFORM_EXPORT AccelerationStructure
		{
		protected:
			crossplatform::RenderPlatform *renderPlatform=nullptr;
			crossplatform::Mesh *mesh=nullptr;
			bool initialized=false;
		public:
			AccelerationStructure(crossplatform::RenderPlatform *r);
			virtual ~AccelerationStructure();
			virtual void RestoreDeviceObjects(Mesh *mesh);
			virtual void InvalidateDeviceObjects();
			virtual void RuntimeInit(DeviceContext &deviceContext){initialized=true;}
			bool IsInitialized() const
			{
				return initialized;
			}
		};
	}
}