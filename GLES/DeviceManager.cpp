#ifdef _MSC_VER
#include <stdlib.h>
#endif
#include "DeviceManager.h"

#include "Platform/CrossPlatform/Camera.h"
#include "Platform/GLES/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/HdrRenderer.h"
#include "Platform/Core/Timer.h"
#include <stdint.h> // for uintptr_t
#include <iomanip>
#include "GLFW/glfw3.h"
#ifdef _MSC_VER
#define GLFW_EXPOSE_NATIVE_WGL
#endif
#include <GLFW/glfw3native.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace simul;
using namespace gles;

using namespace simul;
using namespace gles;
using namespace std;

static std::vector<std::string> debugMsgGroups;
static void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei lenght, const char* message, const void* userParam)
{
    // Source:
    std::string sourceText = "";
    switch (source)
    {
    case GL_DEBUG_SOURCE_API:
        sourceText = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        sourceText = "WINDOW";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        sourceText = "SHADER_COMPILER";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        sourceText = "3RD_PARTY";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        sourceText = "APP";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        sourceText = "OTHER";
        break;
    default:
        sourceText = "NULL";
        break;
    }
    // Type:
    std::string typeText = "";
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        typeText = "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        typeText = "DEPRECATED";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        typeText = "UNDEFINED";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        typeText = "PORTABILITY";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        typeText = "PERFORMANCE";
        break;
    case GL_DEBUG_TYPE_MARKER:
        typeText = "MARKER";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        typeText = "PUSH";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        typeText = "POP";
        break;
    case GL_DEBUG_TYPE_OTHER:
        typeText = "OTHER";
        break;
    default:
        typeText = "NULL";
        break;
    }

    bool ignoreNotifications = true;

    // Severity:
    std::string severityText = "";
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        severityText = "HIGH";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        severityText = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        severityText = "LOW";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        severityText = "NOTIFICATION";
        if (ignoreNotifications)
        {
            return;
        }
        break;
    default:
        severityText = "NULL";
        break;
    }
    if (type == GL_DEBUG_TYPE_PUSH_GROUP)
    {
        debugMsgGroups.push_back(message);
        return;
    }
    else if (type == GL_DEBUG_TYPE_POP_GROUP)
    {
        debugMsgGroups.pop_back();
        return;
    }

    // Print the message:
    if (!debugMsgGroups.empty())
    {
        std::cout << "[GL " << typeText.c_str() << "] (SEVERITY: " << severityText.c_str() << ") CAUSED BY: " << sourceText.c_str() << ", GROUP(" << debugMsgGroups[debugMsgGroups.size()-1].c_str() << ") " << message << std::endl;
    }
    else
    {
        std::cout << "[GL " << typeText.c_str() << "] (SEVERITY: " << severityText.c_str() << ") CAUSED BY: " << sourceText.c_str() << ", " << message << std::endl;
    }
    if (type != GL_DEBUG_TYPE_PERFORMANCE)
    {
        SIMUL_BREAK("");
    }
}



DeviceManager::DeviceManager()
	://renderPlatformOpenGL(NULL)
	offscreen_context(nullptr)
    #ifdef _MSC_VER
	,hRC(nullptr)
    #endif
{
	//if(!renderPlatformOpenGL)
		//renderPlatformOpenGL		=new gles::RenderPlatform;
	//renderPlatformOpenGL->SetShaderBuildMode(crossplatform::BUILD_IF_CHANGED|crossplatform::TRY_AGAIN_ON_FAIL|crossplatform::BREAK_ON_FAIL);
	//simul::gles::Profiler::GetGlobalProfiler().Initialize(NULL);

}

void DeviceManager::InvalidateDeviceObjects()
{
	int err=errno;
	std::cout<<"Errno "<<err<<std::endl;
	errno=0;
/*	if(hRC)
glfw owns hRC
	if(!wglDeleteContext(hRC))
	{
		SIMUL_CERR<<"wglDeleteContext Failed."<<std::endl;
		r*eturn;
	}*/
    #ifdef _MSC_VER
	hRC=nullptr;    
    #endif                       // Set DC To NULL
ERRNO_CHECK
	//simul::gles::Profiler::GetGlobalProfiler().Uninitialize();
	//glfwMakeContextCurrent(nullptr);
   // glfwDestroyWindow(offscreen_context);
	offscreen_context=nullptr;
    //glfwTerminate();
	//delete renderPlatformOpenGL;
	//renderPlatformOpenGL=nullptr;
}

DeviceManager::~DeviceManager()
{
	InvalidateDeviceObjects();
	//delete renderPlatformOpenGL;
}

static void GlfwErrorCallback(int errcode, const char* info)
{
    SIMUL_CERR << " "<<errcode<<": " << info << std::endl;
}

bool	DeviceManager::IsActive() const
{
	return true;
}

void DeviceManager::Initialize(bool use_debug, bool instrument, bool default_driver)
{
    if (!glfwInit())
    {
		SIMUL_CERR << "Coult not init glfw"<<std::endl;
        return ;
    }
    glfwSetErrorCallback(GlfwErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
   // glfwWindowHint(GLFW_GLES_PROFILE, GLFW_GLES_CORE_PROFILE);
	// It is profoundly stupid, but glad cannot be initialized without an existing context, i.e. a WINDOW must be created,
	// before we can initialize the FUNCTIONS that gl runs on.
	// So we have to create an invisible one here:

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_RED_BITS, 8);
	glfwWindowHint(GLFW_GREEN_BITS, 8);
	glfwWindowHint(GLFW_BLUE_BITS, 8);
	glfwWindowHint(GLFW_ALPHA_BITS, 8);
	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_STENCIL_BITS, 0);
	
	offscreen_context = glfwCreateWindow(640, 480, "",nullptr,NULL);
	if (!offscreen_context)
    {
        SIMUL_CERR << "[ERROR] Coult not create a glfw window \n";
        return ;
    }
    glfwSetWindowUserPointer(offscreen_context, this);
   
    glfwMakeContextCurrent(offscreen_context);
    #ifdef _MSC_VER
	hRC=glfwGetWGLContext(offscreen_context);
    #endif
	glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
	/*
    if (!angle::())
    {
        SIMUL_CERR << "Could not initialize angle.\n";
        return ;
    }*/
	InitDebugging();
}

void DeviceManager::InitDebugging()
{
    {
        SIMUL_COUT << "[INFO] The GLES device will run with GL_DEBUG_OUTPUT. \n";
    }
   
    #ifdef _MSC_VER
    // Setup the debug callback
	glDebugMessageCallback((GLDEBUGPROC)GLDebugCallback, this);
    #endif
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
}

void	DeviceManager::Shutdown() 
{
	InvalidateDeviceObjects();
}

void*	DeviceManager::GetDevice() 
{
    #ifdef _MSC_VER
	return (void*)    hRC;
    #else
    return nullptr;
    #endif
}

void*	DeviceManager::GetDeviceContext() 
{
    #ifdef _MSC_VER
    return (void*)    hRC;
    #else
    return nullptr;
    #endif
}

int		DeviceManager::GetNumOutputs() 
{
	return 1;
}

crossplatform::Output DeviceManager::GetOutput(int i)
{
	crossplatform::Output o;
	return o;
}


void DeviceManager::Activate()
{
	glfwMakeContextCurrent(offscreen_context);
}

void DeviceManager::Deactivate()
{
	glfwMakeContextCurrent(nullptr); //Need to called in SkySequencer, when closing the GLES view - AJR
}

void DeviceManager::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
}

void DeviceManager::ReloadTextures()
{
}

void DeviceManager::RenderDepthBuffers(crossplatform::GraphicsDeviceContext &GraphicsDeviceContext,int x0,int y0,int dx,int dy)
{
}