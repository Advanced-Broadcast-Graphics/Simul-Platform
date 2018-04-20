#include "FramebufferGL.h"
#include <iostream>
#include <string>
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/OpenGL/RenderPlatform.h"

#ifdef _MSC_VER
    #include <windows.h>
#else
    #define BREAK_IF_DEBUGGING 
#endif

using namespace simul;
using namespace opengl;

FramebufferGL::FramebufferGL(const char* name):
    BaseFramebuffer(name),
    mFBOId(0)
{
    this->name = name;
}

FramebufferGL::~FramebufferGL()
{
    InvalidateDeviceObjects();
}

void FramebufferGL::SetWidthAndHeight(int w, int h, int m)
{
    if (Width != w || Height != h || mips != m)
    {
        Width   = w;
        Height  = h;
        mips    = m;
        InvalidateDeviceObjects();
    }
}

void FramebufferGL::SetAsCubemap(int w, int num_mips, crossplatform::PixelFormat f)
{
    SetWidthAndHeight(w, w, num_mips);
    SetFormat(f);
    is_cubemap = true;
}

void FramebufferGL::SetFormat(crossplatform::PixelFormat f)
{
    if (target_format == f)
    {
        return;
    }
    target_format = f;
    InvalidateDeviceObjects();
}

void FramebufferGL::SetDepthFormat(crossplatform::PixelFormat f)
{
    if ((int)depth_format == f)
    {
        return;
    }
    depth_format = f;
    InvalidateDeviceObjects();
}

bool FramebufferGL::IsValid() const
{
    return ((buffer_texture != nullptr && buffer_texture->IsValid()) || (buffer_depth_texture != nullptr && buffer_depth_texture->IsValid()));
}

void FramebufferGL::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
    renderPlatform = r;

    if (!external_texture)
    {
        SAFE_DELETE(buffer_texture);
    }
    if (!external_depth_texture)
    {
        SAFE_DELETE(buffer_depth_texture);
    }
    if (renderPlatform)
    {
        if (!external_texture)
        {
            std::string sn = "BaseFramebuffer_" + name;
            buffer_texture = renderPlatform->CreateTexture(sn.c_str());
        }
        if (!external_depth_texture)
        {
            std::string sn = "BaseFramebufferDepth_" + name;
            buffer_depth_texture = renderPlatform->CreateTexture(sn.c_str());
        }
    }

    CreateBuffers();
}

void FramebufferGL::ActivateDepth(crossplatform::DeviceContext &deviceContext)
{
}

void FramebufferGL::SetAntialiasing(int s)
{
    numAntialiasingSamples = s;
}

bool FramebufferGL::CreateBuffers()
{
    if (!Width || !Height)
    {
        return false;
    }
    if (!renderPlatform)
    {
        SIMUL_BREAK("renderPlatform should not be NULL here");
        return false;
    }
    if ((buffer_texture && buffer_texture->IsValid()))
    {
        return true;
    }
    if (buffer_depth_texture && buffer_depth_texture->IsValid())
    {
        return true;
    }
    if (buffer_texture)
    {
        buffer_texture->InvalidateDeviceObjects();
    }
    if (buffer_depth_texture)
    {
        buffer_depth_texture->InvalidateDeviceObjects();
    }
    if (!buffer_texture)
    {
        std::string sn = "BaseFramebuffer_" + name;
        buffer_texture = renderPlatform->CreateTexture(sn.c_str());
    }
    if (!buffer_depth_texture)
    {
        std::string sn = "BaseFramebufferDepth_" + name;
        buffer_depth_texture = renderPlatform->CreateTexture(sn.c_str());
    }
    static int quality = 0;
    if (!external_texture && target_format != crossplatform::UNKNOWN)
    {
        if (!is_cubemap)
        {
            buffer_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height, target_format, false, true, false, numAntialiasingSamples, quality);
        }
        else
        {
            buffer_texture->ensureTextureArraySizeAndFormat(renderPlatform, Width, Height, 1, mips, target_format, false, true, true);
        }
    }
    if (!external_depth_texture && depth_format != crossplatform::UNKNOWN)
    {
        buffer_depth_texture->ensureTexture2DSizeAndFormat(renderPlatform, Width, Height, depth_format, false, false, true, numAntialiasingSamples, quality);
    }

    // Generate GL FBO:
    glGenFramebuffers(1, &mFBOId);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOId);
    {
        auto glcolour = (opengl::Texture*)buffer_texture;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glcolour->AsOpenGLView(crossplatform::ShaderResourceType::TEXTURE_2D), 0);
        if (depth_format != crossplatform::UNKNOWN)
        {
            auto gldepth = (opengl::Texture*)buffer_depth_texture;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gldepth->AsOpenGLView(crossplatform::ShaderResourceType::TEXTURE_2D), 0);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return true;
}

void FramebufferGL::InvalidateDeviceObjects()
{
    if (mFBOId != 0)
    {
        glDeleteFramebuffers(1, &mFBOId);
        mFBOId = 0;
    }
    SAFE_DELETE(buffer_texture)
    SAFE_DELETE(buffer_depth_texture)
}

void FramebufferGL::Activate(crossplatform::DeviceContext& deviceContext)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOId);

    colour_active   = true;
    
    opengl::Texture* glcol      = (opengl::Texture*)buffer_texture;
    opengl::Texture* gldepth    = nullptr;
    if (buffer_depth_texture)
    {
        gldepth         = (opengl::Texture*)buffer_depth_texture;
        // Re-attach depth (we dont really to do this every time, only if we called deactivate depth)
        if (depth_active == false)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gldepth->GetGLMainView(), 0);
        }
        depth_active    = true;
    }
    
    // We need to attach the requested face:
    if (is_cubemap)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glcol->AsOpenGLView(crossplatform::ShaderResourceType::TEXTURE_2D, current_face), 0);
    }

    // Construct targets and viewport:
    targetsAndViewport.num              = 1;
    targetsAndViewport.m_rt[0]          = (void*)mFBOId;
    targetsAndViewport.m_dt             = 0;
    targetsAndViewport.viewport.x       = 0;
    targetsAndViewport.viewport.y       = 0;
    targetsAndViewport.viewport.w       = Width;
    targetsAndViewport.viewport.h       = Height;
    targetsAndViewport.viewport.zfar    = 1.0f;
    targetsAndViewport.viewport.znear   = 0.0f;

    deviceContext.renderPlatform->SetViewports(deviceContext, 1, &targetsAndViewport.viewport);

    // Cache it:
    deviceContext.GetFrameBufferStack().push(&targetsAndViewport);
}

void FramebufferGL::SetExternalTextures(crossplatform::Texture* colour, crossplatform::Texture* depth)
{
    BaseFramebuffer::SetExternalTextures(colour, depth);
}

void FramebufferGL::ActivateViewport(crossplatform::DeviceContext &deviceContext, float x, float y, float w, float h)
{
    SIMUL_BREAK("");
}

void FramebufferGL::ActivateColour(crossplatform::DeviceContext& deviceContext, const float viewportXYWH[4])
{
    Activate(deviceContext);
    if (viewportXYWH[0] != 0 || viewportXYWH[1] != 0 ||
        viewportXYWH[2] != Width || viewportXYWH[3] != Height)
    {
        SIMUL_BREAK("");
    }
}

void FramebufferGL::Clear(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a, float d, int mask)
{
    if (!colour_active)
    {
        colour_active;
    }
    // This call must be made within a Activate - Deactivate block!
    glClearColor(r, g, b, a);
    GLenum settings = GL_COLOR_BUFFER_BIT;
    if (buffer_depth_texture)
    {
        glDepthMask(GL_TRUE);
        glClearDepth(d);
        settings |= GL_DEPTH_BUFFER_BIT;
    }
    glClear(settings);
}

void FramebufferGL::ClearColour(crossplatform::DeviceContext &deviceContext, float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void FramebufferGL::Deactivate(crossplatform::DeviceContext& deviceContext)
{
    deviceContext.renderPlatform->DeactivateRenderTargets(deviceContext);
    colour_active   = false;
    depth_active    = false;
}

void FramebufferGL::DeactivateDepth(crossplatform::DeviceContext &deviceContext)
{
    // This call must be made inside Activate - Deactivate block!
    if (depth_active)
    {
        depth_active = false;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    }
}
