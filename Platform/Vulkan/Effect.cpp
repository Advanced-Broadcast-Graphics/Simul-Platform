﻿#include <stdlib.h>
#include <stdio.h>
#include <string>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/Vulkan/Effect.h"
#include "Simul/Platform/Vulkan/RenderPlatform.h"
#include "Simul/Platform/Vulkan/Framebuffer.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/Timer.h"
#include "Simul/Base/CommandLine.h"
#include "Simul/Base/EnvironmentVariables.h"

using namespace simul;
using namespace vulkan;

bool RewriteOutput(std::string str)
{
	std::cerr<<str.c_str();
	return true;
}


void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *)
{
}

void Query::InvalidateDeviceObjects() 
{
}

void Query::Begin(crossplatform::DeviceContext &)
{
}

void Query::End(crossplatform::DeviceContext &)
{
}

bool Query::GetData(crossplatform::DeviceContext &,void *data,size_t )
{
    return false;
}

Effect::Effect()
{
}

void Effect::Compile(const char *filename_utf8)
{
	/* SIMUL/Tools/bin/Sfx.exe  -I"SIMUL\Platform\Vulkan\GLSL;SIMUL\Platform\CrossPlatform\SL"
											-O"SIMUL\Platform\Vulkan\shaderbin"
												-P"SIMUL\Platform\Vulkan\GLSL\GLSL.json"
												-m"SIMUL\Platform\Vulkan\shaderbin" 
												*/

	std::string filename_fx(filename_utf8);
	if(filename_fx.find(".")>=filename_fx.length())
		filename_fx+=".sfx";
	int index=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(filename_fx.c_str(),renderPlatform->GetShaderPathsUtf8());
	filenameInUseUtf8=filename_fx;
	if(index==-2||index>=(int)renderPlatform->GetShaderPathsUtf8().size())
	{
		filenameInUseUtf8=filename_fx;
		SIMUL_CERR<<"Failed to find shader source file "<<filename_utf8<<std::endl;
		return;
	}
	else if(index<renderPlatform->GetShaderPathsUtf8().size())
		filenameInUseUtf8=(renderPlatform->GetShaderPathsUtf8()[index]+"/")+filename_fx;
	std::string SIMUL=base::EnvironmentVariables::GetSimulEnvironmentVariable("SIMUL");
	std::string command="{SIMUL}/Tools/bin/Sfx.exe -I\"{SIMUL}/Platform/Vulkan/GLSL;{SIMUL}/Platform/CrossPlatform/SL\""
											" -O\"{SIMUL}/Platform/Vulkan/shaderbin\""
												" -P\"{SIMUL}/Platform/Vulkan/GLSL/GLSL.json\""
												" -m\"{SIMUL}/Platform/Vulkan/shaderbin\" ";
	command+=filenameInUseUtf8.c_str();
	base::find_and_replace(command,"{SIMUL}",SIMUL);

	base::OutputDelegate cc=std::bind(&RewriteOutput,std::placeholders::_1);
	base::RunCommandLine(command.c_str(),  cc);
}

Effect::~Effect()
{
	platform_effect=0;
}   

EffectTechnique* Effect::CreateTechnique()
{
	return new vulkan::EffectTechnique(renderPlatform,this);
}

crossplatform::EffectTechnique* Effect::GetTechniqueByIndex(int index)
{
    return techniques_by_index[index];
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const char* name, crossplatform::Texture* tex, int index, int mip)
{
    auto res = GetShaderResource(name);
    SetUnorderedAccessView(deviceContext, res, tex, index, mip);
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext,const crossplatform::ShaderResource& name, crossplatform::Texture* tex, int index, int mip)
{
	crossplatform::Effect::SetUnorderedAccessView(deviceContext,name,tex,index,mip);
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext& deviceContext,crossplatform::ConstantBufferBase* s)
{
    RenderPlatform *r = (RenderPlatform *)deviceContext.renderPlatform;
    s->GetPlatformConstantBuffer()->Apply(deviceContext, s->GetSize(), s->GetAddr());

    crossplatform::Effect::SetConstantBuffer(deviceContext, s);
}

void Effect::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,int pass)
{
    crossplatform::Effect::Apply(deviceContext, effectTechnique, pass);
}

void Effect::Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,const char* pass)
{
    crossplatform::Effect::Apply(deviceContext, effectTechnique, pass);
}

void Effect::Reapply(crossplatform::DeviceContext& deviceContext)
{
    if (apply_count != 1)
    {
        SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Reapply can only be called after Apply and before Unapply. Effect: %s\n", this->filename.c_str()));
    }
    apply_count--;
    crossplatform::ContextState *cs = renderPlatform->GetContextState(deviceContext);
    cs->textureAssignmentMapValid = false;
    cs->rwTextureAssignmentMapValid = false;
    crossplatform::Effect::Apply(deviceContext, currentTechnique, currentPass);
}

void Effect::Unapply(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::Effect::Unapply(deviceContext);
}

void Effect::UnbindTextures(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::Effect::UnbindTextures(deviceContext);
}

crossplatform::EffectPass* EffectTechnique::AddPass(const char* name, int i)
{
	crossplatform::EffectPass* p    = new vulkan::EffectPass(renderPlatform,effect);
	p->SetName(name);
	passes_by_name[name]            = passes_by_index[i] = p;
	return p;
}

TexHandlesUBO::TexHandlesUBO()
  //  mId(0)
{
}


TexHandlesUBO::~TexHandlesUBO()
{
    Release();
}

/*
void TexHandlesUBO::Init(size_t count, GLuint program, int index, int slot)
{
    Release();

	size=count;
    mSlot = slot;
}

void TexHandlesUBO::Bind(GLuint program)
{
}

void TexHandlesUBO::Update(GLuint64 value, size_t offset)
{
	if(offset/sizeof(GLuint64)>=size)
		SIMUL_BREAK("");
}
*/
void TexHandlesUBO::Release()
{
 //   if (mId != 0)
    {
  //      mId = 0;
    }
}
