#include "AccelerationStructure.h"
using namespace simul;
using namespace crossplatform;

AccelerationStructure::AccelerationStructure(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
}
AccelerationStructure::~AccelerationStructure()
{
	InvalidateDeviceObjects();
}
void AccelerationStructure::RestoreDeviceObjects(Mesh *mesh)
{
}
void AccelerationStructure::InvalidateDeviceObjects()
{
}