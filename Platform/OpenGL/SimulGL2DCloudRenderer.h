// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseCloudRenderer.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Platform/OpenGL/Export.h"
namespace simul
{
	namespace clouds
	{
		class CloudInterface;
		class Cloud2DGeometryHelper;
		class CloudKeyframer;
		class FastCloudNode;
	}
	namespace sky
	{
		class SkyInterface;
		class FadeTableInterface;
	}
}

SIMUL_OPENGL_EXPORT_CLASS SimulGL2DCloudRenderer : public simul::clouds::BaseCloudRenderer
{
public:
	SimulGL2DCloudRenderer();
	virtual ~SimulGL2DCloudRenderer();
	void SetSkyInterface(simul::sky::SkyInterface *si);
	void SetFadeTable(simul::sky::FadeTableInterface *fti);
	//standard ogl object interface functions
	bool Create();
	//! OpenGL Implementation of device object creation - needs a GL context to be present.
	bool RestoreDeviceObjects(void*);
	//! OpenGL Implementation of device invalidation - not strictly needed in GL.
	bool InvalidateDeviceObjects();
	//! OpenGL Implementation of 2D cloud rendering.
	bool Render(bool cubemap,bool depth_testing,bool default_fog);
	void SetWindVelocity(float x,float y);
	simul::clouds::CloudInterface *GetCloudInterface();
	void SetFadeTableInterface(simul::sky::FadeTableInterface *fti)
	{
		fadeTableInterface=fti;
	}
	// implementing CloudRenderCallback:
	void SetCloudTextureSize(unsigned width_x,unsigned length_y,unsigned depth_z);
	void FillCloudTexture(int texture_index,int texel_index,int num_texels,const unsigned *uint32_array);
	void CycleTexturesForward();
	bool IsYVertical() const{return false;}
protected:
	simul::clouds::CloudInterface *cloudInterface;
	simul::sky::SkyInterface *skyInterface;
	simul::sky::FadeTableInterface *fadeTableInterface;
	simul::base::SmartPtr<simul::clouds::Cloud2DGeometryHelper> helper;
	simul::base::SmartPtr<simul::clouds::CloudKeyframer> cloudKeyframer;
	GLuint clouds_vertex_shader,clouds_fragment_shader;
	GLuint clouds_program;

	GLint eyePosition_param;
	GLint lightResponse_param;
	GLint lightDir_param;
	GLint skylightColour_param;
	GLint sunlightColour_param;
	GLint fractalScale_param;
	GLint interp_param;
	GLint layerDensity_param;
	GLint textureEffect_param;

	GLuint	cloud_tex[3];
	GLuint	noise_tex;
	GLuint	image_tex;
	float	cam_pos[3];

	bool CreateNoiseTexture();
	bool CreateImageTexture();
	bool CreateCloudEffect();
	bool RenderCloudsToBuffer();
	simul::base::SmartPtr<simul::clouds::FastCloudNode> cloudNode;

	float texture_scale;
	float scale;
	float texture_effect;
	unsigned char *cloud_data;
	unsigned tex_width;
};

