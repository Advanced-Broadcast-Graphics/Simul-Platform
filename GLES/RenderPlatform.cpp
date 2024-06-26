﻿

#include "Platform/GLES/RenderPlatform.h"
#include "Platform/GLES/Mesh.h"
#include "Platform/GLES/Texture.h"
#include "Platform/GLES/Effect.h"
#include "Platform/GLES/Light.h"
#include "Platform/GLES/Buffer.h"
#include "Platform/GLES/FramebufferGL.h"
#include "Platform/GLES/Layout.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/GLES/Texture.h"
#include "Platform/GLES/DisplaySurface.h"
#define GLFW_INCLUDE_ES31
#include <GLFW/glfw3.h>
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>

using namespace simul;
using namespace gles;

RenderPlatform::RenderPlatform():
    mNullVAO(0)
	,mDummy2D(nullptr)
    ,mDummy3D(nullptr)
{
    mirrorY     = true;
    mirrorY2    = true;
    mirrorYText = true;

    mCachedState.Vao            = 0;
    mCachedState.Program        = 0;
    mCachedState.Framebuffer    = 0;
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

const char* RenderPlatform::GetName()const
{
    return "GLES";
}

void RenderPlatform::RestoreDeviceObjects(void* hrc)
{

    // Generate and bind a dummy vao:
    glGenVertexArraysOES(1, &mNullVAO);
    glBindVertexArrayOES(mNullVAO);

    // Query limits:
    glGetIntegerv(GL_MAX_VIEWPORTS_OES, &mMaxViewports);
    //glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &mMaxColorAttatch);
	mMaxColorAttatch=1;

    crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);

	immediateContext.platform_context=hrc;
    RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
    // glDeleteVertexArrays(1, &mNullVAO);
	for(int i=0;i<3;i++)
	{
		for(auto t:texturesToDelete[i])
		{
			glDeleteTextures(1,&t);
		}
		texturesToDelete[i].clear();
	}
}

void RenderPlatform::BeginFrame(crossplatform::GraphicsDeviceContext& deviceContext)
{
	crossplatform::RenderPlatform::BeginFrame(deviceContext);
	for(auto t:texturesToDelete[mCurIdx])
	{
		glDeleteTextures(1,&t);
	}
	if(texturesToDelete[mCurIdx].size())
		ClearResidentTextures();
	texturesToDelete[mCurIdx].clear();
}

void RenderPlatform::EndFrame(crossplatform::GraphicsDeviceContext& deviceContext)
{
	crossplatform::RenderPlatform::EndFrame(deviceContext);
}

void RenderPlatform::BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)
{
    glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION_KHR, 123, -1, name);
}

void RenderPlatform::EndEvent(crossplatform::DeviceContext& deviceContext)
{
    glPopDebugGroupKHR();
}

void RenderPlatform::StoreGLState()
{
    //glGetIntegerv(GL_VERTEX_ARRAY_BINDING,      &mCachedState.Vao);
    //glGetIntegerv(GL_CURRENT_PROGRAM,           &mCachedState.Program);
    //glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,  &mCachedState.Framebuffer); //draw, read col0,1 etc -_-

    // TO-DO: blend,depth,raster

    // Lets bind our dummy vao
    //glBindVertexArray(mNullVAO);
}

void RenderPlatform::RestoreGLState()
{
    //glBindVertexArray(mCachedState.Vao);
    //glUseProgram(mCachedState.Program);
    //glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mCachedState.Framebuffer);
}

void RenderPlatform::ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* texture)
{
	//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void RenderPlatform::ResourceBarrierUAV(crossplatform::DeviceContext& deviceContext, crossplatform::PlatformStructuredBuffer* sb)
{
	//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void RenderPlatform::DispatchCompute(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	const char* effectPass = ((gles::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str();
    BeginEvent(deviceContext, effectPass);
    ApplyCurrentPass(deviceContext);
    glDispatchCompute(w, l, d);
    InsertFences(deviceContext);
    EndEvent(deviceContext);
}

void RenderPlatform::DrawQuad(crossplatform::GraphicsDeviceContext& deviceContext)   
{
    BeginEvent(deviceContext, ((gles::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
    ApplyCurrentPass(deviceContext);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EndEvent(deviceContext);
}

void RenderPlatform::ApplyCurrentPass(crossplatform::DeviceContext & deviceContext)
{
	if (mLastFrame != deviceContext.frame_number)
	{
		mLastFrame = deviceContext.frame_number;
		mCurIdx++;
		mCurIdx = mCurIdx % kNumIdx;
		if(deviceContext.AsGraphicsDeviceContext())
			BeginFrame(*deviceContext.AsGraphicsDeviceContext());
	}

    crossplatform::ContextState* cs = &deviceContext.contextState;
	gles::EffectPass* pass = (gles::EffectPass*)cs->currentEffectPass;
	
	pass->SetTextureHandles(deviceContext);
}

void RenderPlatform::ApplyPass(crossplatform::DeviceContext& deviceContext, crossplatform::EffectPass* pass)
{
	crossplatform::ContextState& cs = deviceContext.contextState;
	if (cs.apply_count == 0)
	{
		crossplatform::RenderPlatform::ApplyPass(deviceContext, pass);
		cs.currentEffectPass->Apply(deviceContext, true);
	}
}

void RenderPlatform::InsertFences(crossplatform::DeviceContext& deviceContext)
{
    auto pass = (gles::EffectPass*)deviceContext.contextState.currentEffectPass;

    if (pass->usesRwSBs())
    {
        for (int i = 0; i < pass->numRwSbResourceSlots; i++)
        {
            int slot    = pass->rwSbResourceSlots[i];
            auto rwsb   = (gles::PlatformStructuredBuffer*)deviceContext.contextState.applyRwStructuredBuffers[slot];
            if (rwsb && pass->usesRwTextureSlotForSB(slot))
            {
                rwsb->AddFence(deviceContext);
            }
        }
    }
	if (pass->usesRwTextures())
	{
		//glMemoryBarrier(GL_ALL_BARRIER_BITS);
	}
}

crossplatform::Mesh* RenderPlatform::CreateMesh()
{
	return new gles::Mesh(this);
}

crossplatform::Light* RenderPlatform::CreateLight()
{
	return new gles::Light();
}

crossplatform::Texture* RenderPlatform::createTexture()
{
	crossplatform::Texture* tex= new gles::Texture();
	return tex;
}

crossplatform::BaseFramebuffer* RenderPlatform::CreateFramebuffer(const char *n)
{
	gles::FramebufferGL* b=new gles::FramebufferGL(n);
	return b;
}

GLenum simul::gles::RenderPlatform::toGLMinFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
    if (f == simul::crossplatform::SamplerStateDesc::LINEAR)
    {
        return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_NEAREST_MIPMAP_NEAREST;
}

GLenum simul::gles::RenderPlatform::toGLMaxFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
    if (f == simul::crossplatform::SamplerStateDesc::LINEAR)
    {
        return GL_LINEAR;
    }
    return GL_NEAREST;
}

GLint RenderPlatform::toGLWrapping(crossplatform::SamplerStateDesc::Wrapping w)
{
	switch(w)
	{
	case crossplatform::SamplerStateDesc::WRAP:
		return GL_REPEAT;
		break;
	case crossplatform::SamplerStateDesc::CLAMP:
		return GL_CLAMP_TO_EDGE;
		break;
	case crossplatform::SamplerStateDesc::MIRROR:
		return GL_MIRRORED_REPEAT;
		break;
	}
	return GL_REPEAT;
}

crossplatform::SamplerState* RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc* desc)
{
	gles::SamplerState* s = new gles::SamplerState();
    s->Init(desc);
	return s;
}

crossplatform::Effect* RenderPlatform::CreateEffect()
{
	gles::Effect* e=new gles::Effect();
	return e;
}

crossplatform::Effect* RenderPlatform::CreateEffect(const char *filename_utf8)
{
	crossplatform::Effect* e=crossplatform::RenderPlatform::CreateEffect(filename_utf8);
	return e;
}

crossplatform::PlatformConstantBuffer* RenderPlatform::CreatePlatformConstantBuffer()
{
	return new gles::PlatformConstantBuffer();
}

crossplatform::PlatformStructuredBuffer* RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer* b=new gles::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer* RenderPlatform::CreateBuffer()
{
	return new gles::Buffer();
}

GLuint RenderPlatform::ToGLInternalFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return GL_R11F_G11F_B10F;
	case RGBA_16_FLOAT:
		return GL_RGBA16F;
	case RGBA_32_FLOAT:
		return GL_RGBA32F;
	case RGBA_32_UINT:
		return GL_RGBA32UI;
	case RGB_32_FLOAT:
		return GL_RGB32F;
	case RG_32_FLOAT:
		return GL_RG32F;
	case R_32_FLOAT:
		return GL_R32F;
	case R_16_FLOAT:
		return GL_R16F;
	case RGBA_8_UNORM:
	case BGRA_8_UNORM:
		return GL_RGBA8;
	case RGBA_8_UNORM_SRGB:
		return GL_SRGB8_ALPHA8;
	case RGBA_8_SNORM:
		return GL_RGBA8_SNORM;
	case RGB_8_UNORM:
		return GL_RGB8;
	case RGB_8_SNORM:
		return GL_RGB8_SNORM;
	case R_8_UNORM:
		return GL_R8;
	case R_32_UINT:
		return GL_R32UI;
	case RG_32_UINT:
		return GL_RG32UI;
	case RGB_32_UINT:
		return GL_RGB32UI;
	case D_32_FLOAT:
		return GL_DEPTH_COMPONENT32F;
	case D_24_UNORM_S_8_UINT:
		return GL_DEPTH_COMPONENT24;
	case D_16_UNORM:
		return GL_DEPTH_COMPONENT16;
	default:
		return 0;
	};
}

crossplatform::PixelFormat RenderPlatform::FromGLInternalFormat(GLuint p)
{
	using namespace crossplatform;
	switch(p)
	{
		case GL_RGBA16F:
			return RGBA_16_FLOAT;
		case GL_RGBA32F:
			return RGBA_32_FLOAT;
		case GL_RGBA32UI:
			return RGBA_32_UINT;
		case GL_RGB32F:
			return RGB_32_FLOAT;
		case GL_RG32F:
			return RG_32_FLOAT;
		case GL_R32F:
			return R_32_FLOAT;
		case GL_R16F:
			return R_16_FLOAT;
		case GL_RGBA8:
			return RGBA_8_UNORM;
		case GL_SRGB8_ALPHA8:
			return RGBA_8_UNORM_SRGB;
		case GL_RGBA8_SNORM:
			return RGBA_8_SNORM;
		case GL_RGB8:
			return RGB_8_UNORM;
		case GL_RGB8_SNORM:
			return RGB_8_SNORM;
		case GL_R8:
			return R_8_UNORM;
		case GL_R32UI:
			return R_32_UINT;
		case GL_RG32UI:
			return RG_32_UINT;
		case GL_RGB32UI:
			return RGB_32_UINT;
		case GL_DEPTH_COMPONENT32F:
			return D_32_FLOAT;
		case GL_DEPTH_COMPONENT24:
			return D_24_UNORM_S_8_UINT;
		case GL_DEPTH_COMPONENT16:
			return D_16_UNORM;
	default:
		return UNKNOWN;
	};
}

GLuint RenderPlatform::ToGLFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return GL_RGB;
	case RGBA_16_FLOAT:
		return GL_RGBA;
	case RGBA_32_FLOAT:
		return GL_RGBA;
	case RGB_32_FLOAT:
		return GL_RGB;
	case RGBA_32_UINT:
		return GL_RGBA_INTEGER;
	case R_32_UINT:
		return GL_RED_INTEGER;
	case RG_32_UINT:
		return GL_RG_INTEGER;
	case RGB_32_UINT:
		return GL_RGB_INTEGER;
	case RG_32_FLOAT:
		return GL_RG;
	case R_16_FLOAT:
		return GL_RED;
	case R_32_FLOAT:
		return GL_RED;
	case LUM_32_FLOAT:
		return GL_RGBA;
	case INT_32_FLOAT:
		return GL_RGBA;
	case RGBA_8_UNORM:
		return GL_RGBA;
	case RGBA_8_UNORM_SRGB:
		return GL_SRGB8_ALPHA8;
	case RGBA_8_SNORM:
		return GL_RGBA;
	case RGB_8_UNORM:
		return GL_RGB;
	case RGB_8_SNORM:
		return GL_RGB;
	case R_8_UNORM:
		return GL_RED;// not GL_R...!
	case D_32_FLOAT:
		return GL_DEPTH_COMPONENT;
	case D_24_UNORM_S_8_UINT:
		return GL_DEPTH_COMPONENT24;
	case D_16_UNORM:
		return GL_DEPTH_COMPONENT;
	default:
		return GL_RGBA;
	};
}

int RenderPlatform::FormatCount(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return 3;
	case RGBA_16_FLOAT:
		return 4;
	case RGBA_32_FLOAT:
		return 4;
	case RGB_32_FLOAT:
		return 3;
	case RG_32_FLOAT:
		return 2;
	case R_32_FLOAT:
		return 1;
	case R_16_FLOAT:
		return 1;
	case LUM_32_FLOAT:
		return 1;
	case INT_32_FLOAT:
		return 1;
	case RGBA_8_UNORM:
	case BGRA_8_UNORM:
	case RGBA_8_UNORM_SRGB:
	case RGBA_8_SNORM:
		return 4;
	case RGB_8_UNORM:
	case RGB_8_SNORM:
		return 3;
	case R_8_UNORM:
	case R_8_SNORM:
	case R_32_UINT:
		return 1;
	case RG_32_UINT:
		return 2;
	case RGB_32_UINT:
		return 3;
	case RGBA_32_UINT:
		return 4;
	case D_32_FLOAT:
		return 1;
	case D_16_UNORM:
		return 1;
	case D_24_UNORM_S_8_UINT:
		return 3;
	default:
		return 0;
	};
}

void RenderPlatform::ClearResidentTextures()
{
	}
 std::set<GLuint>	RenderPlatform::texturesToDelete[3];

void RenderPlatform::DeleteGLTextures(const std::set<GLuint> &t)
{
	for(auto c:t)
	{
		texturesToDelete[mCurIdx].insert(c);
	}
}

void RenderPlatform::MakeTextureResident(GLuint64 handle)
{
    }

const float whiteTexel[4] = { 1.0f,1.0f,1.0f,1.0f};

gles::Texture* RenderPlatform::GetDummy2D()
{
    if (!mDummy2D)
    {
        mDummy2D = (gles::Texture*)CreateTexture("dummy2d");
        mDummy2D->ensureTexture2DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM);
        mDummy2D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
    }
    return mDummy2D;
}

gles::Texture* RenderPlatform::GetDummy3D()
{
    if (!mDummy3D)
    {
        mDummy3D = (gles::Texture*)CreateTexture("dummy3d");
        mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM);
        mDummy3D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
    }
    return mDummy3D;
}

GLenum RenderPlatform::DataType(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return GL_FLOAT;
	case RGBA_16_FLOAT:
		return GL_FLOAT;
	case RGBA_32_FLOAT:
		return GL_FLOAT;
	case RGB_32_FLOAT:
		return GL_FLOAT;
	case RG_32_FLOAT:
		return GL_FLOAT;
	case R_32_FLOAT:
		return GL_FLOAT;
	case R_16_FLOAT:
		return GL_FLOAT;
	case LUM_32_FLOAT:
		return GL_FLOAT;
	case INT_32_FLOAT:
		return GL_FLOAT;
	case RGBA_8_UNORM_SRGB:
	case BGRA_8_UNORM:
	case RGBA_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGBA_8_SNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_SNORM:
		return GL_UNSIGNED_INT;
	case R_8_UNORM:
		return GL_UNSIGNED_BYTE;
	case R_8_SNORM:
		return GL_UNSIGNED_BYTE;
	case R_32_UINT:
		return GL_UNSIGNED_INT;
	case RG_32_UINT:
		return GL_UNSIGNED_INT;
	case RGB_32_UINT:
		return GL_UNSIGNED_INT;
	case RGBA_32_UINT:
		return GL_UNSIGNED_INT;
	case D_32_FLOAT:
		return GL_FLOAT;
	case D_16_UNORM:
		return GL_UNSIGNED_SHORT;
	case D_24_UNORM_S_8_UINT:
		return GL_UNSIGNED_INT_24_8;
	default:
		return 0;
	};
}

crossplatform::Layout* RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc,bool in)
{
    gles::Layout* l = new gles::Layout();
    l->SetDesc(desc, num_elements, in);
	return l;
}

crossplatform::RenderState* RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	gles::RenderState* s  = new gles::RenderState;
	s->desc                 = desc;
	s->type                 = desc.type;
	return s;
}

crossplatform::Query* RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	gles::Query* q=new gles::Query(type);
	return q;
}

static GLenum toGlCullFace(crossplatform::CullFaceMode c)
{
    switch (c)
    {
    case simul::crossplatform::CULL_FACE_FRONT:
        return GL_FRONT;
    case simul::crossplatform::CULL_FACE_BACK:
        return GL_BACK;
    case simul::crossplatform::CULL_FACE_FRONTANDBACK:
        return GL_FRONT_AND_BACK;
    default:
        break;
    }
    return GL_FRONT;
}

static GLenum toGlFun(crossplatform::BlendOperation o)
{
    switch (o)
    {
    case simul::crossplatform::BLEND_OP_ADD:
        return GL_FUNC_ADD;
    case simul::crossplatform::BLEND_OP_SUBTRACT:
        return GL_FUNC_SUBTRACT;
    case simul::crossplatform::BLEND_OP_MAX:
        return GL_MAX;
    case simul::crossplatform::BLEND_OP_MIN:
        return GL_MIN;
    default:
        break;
    }
    return GL_FUNC_ADD;
}

static GLenum toGlComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_ALWAYS:
		return GL_ALWAYS;
	case crossplatform::DEPTH_LESS:
		return GL_LESS;
	case crossplatform::DEPTH_EQUAL:
		return GL_EQUAL;
	case crossplatform::DEPTH_LESS_EQUAL:
		return GL_LEQUAL;
	case crossplatform::DEPTH_GREATER:
		return GL_GREATER;
	case crossplatform::DEPTH_NOT_EQUAL:
		return GL_NOTEQUAL;
	case crossplatform::DEPTH_GREATER_EQUAL:
		return GL_GEQUAL;
	default:
		break;
	};
	return GL_LESS;
}

static GLenum toGlBlendOp(crossplatform::BlendOption o)
{
	switch(o)
	{
	case crossplatform::BLEND_ZERO:
		return GL_ZERO;
	case crossplatform::BLEND_ONE:
		return GL_ONE;
	case crossplatform::BLEND_SRC_COLOR:
		return GL_SRC_COLOR;
	case crossplatform::BLEND_INV_SRC_COLOR:
		return GL_ONE_MINUS_SRC_COLOR;
	case crossplatform::BLEND_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case crossplatform::BLEND_INV_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case crossplatform::BLEND_DEST_ALPHA:
		return GL_DST_ALPHA;
	case crossplatform::BLEND_INV_DEST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case crossplatform::BLEND_DEST_COLOR:
		return GL_DST_COLOR;
	case crossplatform::BLEND_INV_DEST_COLOR:
		return GL_ONE_MINUS_DST_COLOR;
	case crossplatform::BLEND_SRC_ALPHA_SAT:
		return 0;
	case crossplatform::BLEND_BLEND_FACTOR:
		return 0;
	case crossplatform::BLEND_INV_BLEND_FACTOR:
		return 0;
	case crossplatform::BLEND_SRC1_COLOR:
		return GL_SRC1_COLOR_EXT;
	case crossplatform::BLEND_INV_SRC1_COLOR:
		return GL_ONE_MINUS_SRC1_COLOR_EXT;
	case crossplatform::BLEND_SRC1_ALPHA:
		return GL_SRC1_ALPHA_EXT;
	case crossplatform::BLEND_INV_SRC1_ALPHA:
		return GL_ONE_MINUS_SRC1_ALPHA_EXT;
	default:
		break;
	};
	return GL_ONE;
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext& deviceContext,const crossplatform::RenderState* s)
{
    auto state = (gles::RenderState*)s;
    if (state->type == crossplatform::RenderStateType::BLEND)
    {
        crossplatform::BlendDesc bdesc = state->desc.blend;
        // We need to iterate over all the rts as we may have some settings
        // from older passes:
        const int kBlendMaxRt = 8;
        for (int i = 0; i < kBlendMaxRt; i++)
        {
            if (i >= bdesc.numRTs || bdesc.RenderTarget[i].blendOperation == crossplatform::BlendOperation::BLEND_OP_NONE)
            {
                glDisableiNV(GL_BLEND, i);
            }
            else
            {
                glEnableiNV(GL_BLEND, i);
                glBlendEquationSeparate(toGlFun(bdesc.RenderTarget[i].blendOperation), toGlFun(bdesc.RenderTarget[i].blendOperationAlpha));
                glBlendFuncSeparate
                (
                    toGlBlendOp(bdesc.RenderTarget[i].SrcBlend), toGlBlendOp(bdesc.RenderTarget[i].DestBlend),
                    toGlBlendOp(bdesc.RenderTarget[i].SrcBlendAlpha), toGlBlendOp(bdesc.RenderTarget[i].DestBlendAlpha)
                );
                unsigned char msk = bdesc.RenderTarget[i].RenderTargetWriteMask;
              /*  glColorMaskiNV
                (
                    i, 
                    (GLboolean)(msk & (1 << 0)), 
                    (GLboolean)(msk & (1 << 1)), 
                    (GLboolean)(msk & (1 << 2)), 
                    (GLboolean)(msk & (1 << 3))
                );*/
            }
        }
    }
    else if (state->type == crossplatform::RenderStateType::DEPTH)
    {
        crossplatform::DepthStencilDesc ddesc = state->desc.depth;
        glDepthMask((GLboolean)ddesc.write);
        if (ddesc.test)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(toGlComparison(ddesc.comparison));
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    }
    else if (state->type == crossplatform::RenderStateType::RASTERIZER)
    {
        crossplatform::RasterizerDesc rdesc = state->desc.rasterizer;
        if (rdesc.cullFaceMode == crossplatform::CullFaceMode::CULL_FACE_NONE)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(toGlCullFace(rdesc.cullFaceMode));
        }
        // Reversed
        glFrontFace(rdesc.frontFace == crossplatform::FrontFace::FRONTFACE_CLOCKWISE ? GL_CCW : GL_CW);
    }
    else
    {
        SIMUL_CERR << "Trying to set an invalid render state \n";
    }
}

void RenderPlatform::SetStandardRenderState(crossplatform::DeviceContext& deviceContext, crossplatform::StandardRenderState s)
{
    SetRenderState(deviceContext, standardRenderStates[s]);
}

void RenderPlatform::Resolve(crossplatform::GraphicsDeviceContext &,crossplatform::Texture *destination,crossplatform::Texture *source)
{
	auto dst = (gles::Texture*)destination;
	auto src = (gles::Texture*)source;
	GLuint srcFBO, dstFBO;
	GLenum fboStatus;

	glGenFramebuffers(1, &srcFBO);
	glGenFramebuffers(1, &dstFBO);

	glBindFramebuffer(GL_FRAMEBUFFER, srcFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, src->AsOpenGLView(crossplatform::ShaderResourceType::TEXTURE_2D|crossplatform::ShaderResourceType::MS), 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		SIMUL_CERR << "FBO is not complete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, dstFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst->AsOpenGLView(crossplatform::ShaderResourceType::TEXTURE_2D), 0);
	fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
		SIMUL_CERR << "FBO is not complete!\n";
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFBO);
	glBlitFramebuffer(0, 0, source->width, source->length, 0, 0, destination->width, destination->length, GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glDeleteFramebuffers(1, &srcFBO);
	glDeleteFramebuffers(1, &dstFBO);
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
}

void RenderPlatform::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& res, crossplatform::Texture* tex, int index, int mip)
{
	gles::Effect* effect = reinterpret_cast<gles::Effect*>(deviceContext.contextState.currentEffect);
	effect->SetUnorderedAccessView(deviceContext, res, tex, index, mip);

	crossplatform::RenderPlatform::SetUnorderedAccessView(deviceContext, res, tex, index, mip);
}

void* RenderPlatform::GetDevice()
{
	return nullptr;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext& deviceContext, int slot, int num_buffers, const crossplatform::Buffer* const* buffers, const crossplatform::Layout* layout, const int* vertexSteps)
{
    if (!buffers)
    {
        return;
    }
    for (int i = 0; i < num_buffers; i++)
    {
        gles::Buffer* glBuffer = (gles::Buffer*)buffers[i];
        if (glBuffer)
        {
            glBuffer->BindVBO(deviceContext);
        }
    }
}

void RenderPlatform::SetStreamOutTarget(crossplatform::GraphicsDeviceContext&,crossplatform::Buffer *buffer,int/* start_index*/)
{
}

void RenderPlatform::ActivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext,int num,crossplatform::Texture** targs,crossplatform::Texture* depth)
{
    if (num >= mMaxColorAttatch)
    {
        SIMUL_CERR << "Too many targets \n";
        return;
    }
}
#include <cstdint>
void RenderPlatform::DeactivateRenderTargets(crossplatform::GraphicsDeviceContext& deviceContext)
{
    deviceContext.GetFrameBufferStack().pop();

    // Default FBO:
    if (deviceContext.GetFrameBufferStack().empty())
    {
        auto defT = deviceContext.defaultTargetsAndViewport;
		const uintptr_t ll= uintptr_t(defT.m_rt[0]);
		GLuint id = GLuint(ll);
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        SetViewports(deviceContext, 1, &defT.viewport);
    }
    // Plugin FBO:
    else
    {
        auto topRt = deviceContext.GetFrameBufferStack().top();
		uintptr_t ll=uintptr_t(topRt->m_rt[0]);
		GLuint id = GLuint(ll);
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        SetViewports(deviceContext, 1, &topRt->viewport);
    }
}

void RenderPlatform::SetViewports(crossplatform::GraphicsDeviceContext& deviceContext,int num ,const crossplatform::Viewport* vps)
{
    if (num >= mMaxViewports)
    {
        SIMUL_CERR << "Too many viewports \n";
        return;
    }
	crossplatform::RenderPlatform::SetViewports(deviceContext,num,vps);
    for (int i = 0; i < num; i++)
    {
		glViewport( (GLfloat)vps[i].x, (GLfloat)vps[i].y, (GLfloat)vps[i].w, (GLfloat)vps[i].h);
        glScissor( (GLint)vps[i].x, (GLint)vps[i].y,   (GLsizei)vps[i].w, (GLsizei)vps[i].h);
    }
}

void RenderPlatform::SetIndexBuffer(crossplatform::GraphicsDeviceContext &, const crossplatform::Buffer *buffer)
{
}

void RenderPlatform::SetTopology(crossplatform::GraphicsDeviceContext &,crossplatform::Topology t)
{
    mCurTopology = toGLTopology(t);
}

void RenderPlatform::EnsureEffectIsBuilt(const char *)
{
}


crossplatform::DisplaySurface* RenderPlatform::CreateDisplaySurface()
{
    return new gles::DisplaySurface();
}

void RenderPlatform::StoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::RestoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::PopRenderTargets(crossplatform::GraphicsDeviceContext &)
{
}

GLenum RenderPlatform::toGLTopology(crossplatform::Topology t)
{
    switch (t)
    {
    case crossplatform::Topology::POINTLIST:
        return GL_POINTS;
    case crossplatform::Topology::LINELIST:
        return GL_LINES;
    case crossplatform::Topology::LINESTRIP:
        return GL_LINE_STRIP;
    case crossplatform::Topology::TRIANGLELIST:
        return GL_TRIANGLES;
    case crossplatform::Topology::TRIANGLESTRIP:
        return GL_TRIANGLE_STRIP;
    default:
        break;
    };
    return GL_LINE_LOOP;
}

void RenderPlatform::Draw(crossplatform::GraphicsDeviceContext &deviceContext,int num_verts,int start_vert)
{
    BeginEvent(deviceContext, ((gles::EffectPass*)deviceContext.contextState.currentEffectPass)->name.c_str());
    ApplyCurrentPass(deviceContext);
    glDrawArrays(mCurTopology, start_vert, num_verts);
    EndEvent(deviceContext);
}

void RenderPlatform::DrawIndexed(crossplatform::GraphicsDeviceContext &deviceContext,int num_indices,int start_index,int base_vertex)
{
}

void RenderPlatform::GenerateMips(crossplatform::GraphicsDeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx)
{
    t->GenerateMips(deviceContext);
}

crossplatform::Shader* RenderPlatform::CreateShader()
{
	Shader* S = new Shader();
	return S;
}
