#include "Framebuffer.h"
#ifdef _MSC_VER
#include <windows.h>
#endif
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/GpuProfiler.h"
#include "Platform/Math/RandomNumberGenerator.h"
using namespace platform;
using namespace crossplatform;


Framebuffer::Framebuffer(const char *n)
	:Width(0)
	,Height(0)
	,mips(0)
	,numAntialiasingSamples(1)	// no AA by default
	,depth_active(false)
	,colour_active(false)
	,renderPlatform(nullptr)
	,DefaultClearColour(1.0f, 1.0f, 1.0f, 1.0f)
	,DefaultClearDepth(1.0f)
	,DefaultClearStencil(1)
	,GenerateMips(false)
	,is_cubemap(false)
	,current_face(-1)
	,target_format(crossplatform::RGBA_MAX_FLOAT)
	,depth_format(crossplatform::UNKNOWN)
	,activate_count(0)
	,external_texture(false)
	,external_depth_texture(false)
{
	if(n)
		name=n;
}

Framebuffer::~Framebuffer()
{
	InvalidateDeviceObjects();
}

void Framebuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform = r;
	if (!renderPlatform)
		return;
	if (!external_texture && !buffer_texture)
	{
		buffer_texture = renderPlatform->CreateTexture((name + "_Colour").c_str());
	}
	if (!external_depth_texture && depth_format != UNKNOWN && !buffer_depth_texture)
	{
		buffer_depth_texture = renderPlatform->CreateTexture((name + "_Depth").c_str());
	}
	CreateBuffers();
}

void Framebuffer::InvalidateDeviceObjects()
{
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	buffer_texture=NULL;
	buffer_depth_texture=NULL;
	renderPlatform=nullptr;
}

void Framebuffer::Clear(crossplatform::GraphicsDeviceContext &deviceContext, float r, float g, float b, float a, float depth)
{
	if ((!buffer_texture || !buffer_texture->IsValid()) && (!buffer_depth_texture || !buffer_depth_texture->IsValid()))
	{
		CreateBuffers();
	}

	ClearColour(deviceContext, r, g, b, a);
	ClearDepth(deviceContext, depth);
}

void Framebuffer::ClearColour(crossplatform::GraphicsDeviceContext &deviceContext, float r, float g, float b, float a)
{
	if (buffer_texture && buffer_texture->IsValid())
	{
		buffer_texture->ClearColour(deviceContext, {r, g, b, a});
	}
}

void Framebuffer::ClearDepth(crossplatform::GraphicsDeviceContext &deviceContext, float depth)
{
	if (buffer_depth_texture && buffer_depth_texture->IsValid())
	{
		buffer_depth_texture->ClearDepthStencil(deviceContext, depth);
	}
}

void Framebuffer::SetWidthAndHeight(int w,int h,int m)
{
	if(Width!=w||Height!=h||mips!=m)
	{
		Width=w;
		Height=h;
		mips=m;
		if(buffer_texture)
			buffer_texture->InvalidateDeviceObjects();
		if(buffer_depth_texture)
			buffer_depth_texture->InvalidateDeviceObjects();
	}
}

void Framebuffer::SetFormat(crossplatform::PixelFormat f)
{
	if(f==target_format)
		return;
	target_format=f;
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
}

void Framebuffer::SetDepthFormat(crossplatform::PixelFormat f)
{
	if(f==depth_format)
		return;
	depth_format=f;
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
}

void Framebuffer::SetGenerateMips(bool m)
{
	GenerateMips=m;
}

void Framebuffer::SetAsCubemap(int w,int num_mips,crossplatform::PixelFormat f)
{
	SetWidthAndHeight(w,w,num_mips);
	SetFormat(f);
	is_cubemap=true;
}

void Framebuffer::SetCubeFace(int f)
{
	if(!is_cubemap)
	{
		SIMUL_BREAK_ONCE("Setting cube face on non-cubemap framebuffer");
	}
	current_face=f;
}

bool Framebuffer::IsDepthActive() const
{
	return depth_active;
}

bool Framebuffer::IsColourActive() const
{
	return colour_active;
}

bool Framebuffer::IsValid() const
{
	bool ok=(buffer_texture!=NULL)||(buffer_depth_texture!=NULL);
	return ok;
}

void Framebuffer::SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth)
{
	if(buffer_texture==colour&&buffer_depth_texture==depth&&(!colour||(colour->width==Width&&colour->length==Height)))
		return;
	if(!external_texture)
		SAFE_DELETE(buffer_texture);
	if(!external_depth_texture)
		SAFE_DELETE(buffer_depth_texture);
	buffer_texture=colour;
	buffer_depth_texture=depth;

	external_texture=true;
	external_depth_texture=true;
	if(colour)
	{
		Width=colour->width;
		Height=colour->length;
		is_cubemap=(colour->depth==6);
	}
	else if(depth)
	{
		Width=depth->width;
		Height=depth->length;
	}
}

bool Framebuffer::CreateBuffers()
{
	if(!Width||!Height)
		return false;
	if(!renderPlatform)
	{
		SIMUL_BREAK("renderPlatform should not be NULL here");
	}
	if(!renderPlatform)
		return false;
	if((buffer_texture&&buffer_texture->IsValid()))
		return true;
	if(buffer_depth_texture&&buffer_depth_texture->IsValid())
		return true;
	if(buffer_texture)
		buffer_texture->InvalidateDeviceObjects();
	if(buffer_depth_texture)
		buffer_depth_texture->InvalidateDeviceObjects();
	if(!buffer_texture)
	{
		std::string cName = name+"_Colour";
		buffer_texture=renderPlatform->CreateTexture(cName.c_str());
	}
	if(!buffer_depth_texture&&depth_format!=crossplatform::UNKNOWN)
	{
		std::string dName = name+"_Depth";
		buffer_depth_texture=renderPlatform->CreateTexture(dName.c_str());
	}
	static int quality=0;
	if(!external_texture&&target_format!=crossplatform::UNKNOWN)
	{
		if(!is_cubemap)
			buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform,Width,Height,1,target_format,false,true,false,numAntialiasingSamples,quality,false,DefaultClearColour,DefaultClearDepth,DefaultClearStencil);
		else
			buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform,Width,Height,1,mips,target_format,nullptr,false,true,false,true);
	}
	if(!external_depth_texture&&depth_format!=crossplatform::UNKNOWN)
	{
		buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height, 1, depth_format, false, false, true, numAntialiasingSamples, quality, false, vec4(0.0f, 0.0f, 0.0f, 0.0f), DefaultClearDepth,DefaultClearStencil);
	}
	return true;
}
