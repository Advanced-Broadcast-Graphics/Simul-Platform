
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
#include <iostream>
#include <algorithm>

using namespace simul;
using namespace crossplatform;
SamplerState::SamplerState():default_slot(-1),renderPlatform(nullptr)
{
}

SamplerState::~SamplerState()
{
}

Texture::Texture(const char *n)
				:cubemap(false)
				,computable(false)
				,renderTarget(false)
				,external_texture(false)
				,depthStencil(false)
				,unfenceable(false)
				,width(0)
				,length(0)
				,depth(0)
				,arraySize(0)
				,dim(0)
				,mips(1)
				,pixelFormat(crossplatform::UNKNOWN)
				,renderPlatform(NULL)
				,textureLoadComplete(true)
{
	if(n)
		name=n;
}

Texture::~Texture()
{
}

void Texture::InitFromExternalTexture(crossplatform::RenderPlatform *renderPlatform, const TextureCreate *textureCreate)
{
	InitFromExternalTexture2D(renderPlatform, textureCreate->external_texture, textureCreate->srv, textureCreate->w, textureCreate->l, textureCreate->f, textureCreate->make_rt, textureCreate->setDepthStencil, textureCreate->need_srv
		, textureCreate->numOfSamples);
}

void Texture::activateRenderTarget(GraphicsDeviceContext &deviceContext,int array_index,int mip_index )
{
	if (array_index == -1)
	{
		array_index = 0;
	}
	if (mip_index == -1)
	{
		mip_index = 0;
	}
	targetsAndViewport.num							=1;
	targetsAndViewport.m_rt[0]						=nullptr;
	targetsAndViewport.textureTargets[0].texture	=this;
	targetsAndViewport.textureTargets[0].mip		=mip_index;
	targetsAndViewport.textureTargets[0].layer		=array_index;
	targetsAndViewport.m_dt							=nullptr;
	targetsAndViewport.viewport.x					=0;
	targetsAndViewport.viewport.y					=0;
	targetsAndViewport.viewport.w					=std::max(1, (width >> mip_index));
	targetsAndViewport.viewport.h					=std::max(1, (length >> mip_index));
	deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

	// Cache it:
	deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}

void Texture::deactivateRenderTarget(GraphicsDeviceContext &deviceContext)
{
	deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
}

bool Texture::IsCubemap() const
{
	return cubemap;
}

void Texture::SetName(const char *n)
{
	name=n;
}
unsigned long long Texture::GetFence(DeviceContext &deviceContext) const
{
	const auto &i=deviceContext.contextState.fenceMap.find((const Texture*)this);
	if(i!=deviceContext.contextState.fenceMap.end())
		return i->second.label;
	return 0;
}

void Texture::SetFence(DeviceContext &deviceContext,unsigned long long f)
{
	auto i=deviceContext.contextState.fenceMap.find(this);
	if(i!=deviceContext.contextState.fenceMap.end())
	if(i->second.label!=0)
	{
		// This is probably ok. We might be adding to a different part of a texture.
	//	SIMUL_CERR<<"Setting fence when the last one is not cleared yet\n"<<std::endl;
	}
	auto &fence=deviceContext.contextState.fenceMap[this];
	fence.label=f;
	fence.texture=this;
}


void Texture::ClearFence(DeviceContext &deviceContext)
{
	auto i=deviceContext.contextState.fenceMap.find(this);
	if(i!=deviceContext.contextState.fenceMap.end())
		deviceContext.contextState.fenceMap.erase(i);
}

void Texture::InvalidateDeviceObjects()
{
	shouldGenerateMips=false;
	width=length=depth=arraySize=dim=mips=0;
	pixelFormat=PixelFormat::UNKNOWN;
	renderPlatform=nullptr;
	textureLoadComplete=true;
}

bool Texture::EnsureTexture(crossplatform::RenderPlatform* r, crossplatform::TextureCreate* tc)
{
	bool res=false;
	if (tc->d == 2&&tc->arraysize==1)
		res= ensureTexture2DSizeAndFormat(r, tc->w, tc->l, tc->mips, tc->f, tc->computable , tc->make_rt , tc->setDepthStencil , tc->numOfSamples , tc->aa_quality , false ,tc->clear, tc->clearDepth , tc->clearStencil );
	else if(tc->d==2)
		res=ensureTextureArraySizeAndFormat( r, tc->w, tc->l, tc->arraysize, tc->mips, tc->f, tc->computable , tc->make_rt, tc->cubemap ) ;
	else if(tc->d==3)
		res=ensureTexture3DSizeAndFormat(r, tc->w, tc->l, tc->d, tc->f, tc->computable , tc->mips , tc->make_rt) ;
	return res;

}

bool Texture::ensureTexture2DSizeAndFormat(crossplatform::RenderPlatform* renderPlatform, int w, int l,
	crossplatform::PixelFormat f, bool computable , bool rendertarget , bool depthstencil , int num_samples , int aa_quality , bool wrap ,
	vec4 clear , float clearDepth , uint clearStencil )
{
	return ensureTexture2DSizeAndFormat(renderPlatform,  w,  l, 1,
		 f,  computable,  rendertarget,  depthstencil,  num_samples,  aa_quality,  wrap,
		 clear,  clearDepth,  clearStencil);
}