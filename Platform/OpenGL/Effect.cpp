#include <GL/glew.h>
#include <GL/glfx.h>
#ifdef _MSC_VER
#include <windows.h>
#endif
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/CrossPlatform/Texture.h"

using namespace simul;
using namespace opengl;

#define CHECK_PARAM_EXISTS\
	if(loc<0)\
	{\
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: parameter "<<name<<" was not found in GLFX program "<<filename.c_str()<<std::endl;\
		std::cout<<filename.c_str()<<"(1): warning B0001: parameter "<<name<<" was not found."<<filename.c_str()<<std::endl;\
		return;\
	}
#define CHECK_TECH_EXISTS\
	if(currentTechnique==NULL)\
	{\
		std::cout<<__FILE__<<"("<<__LINE__<<"): warning B0001: currentTechnique is NULL in "<<filename.c_str()<<std::endl;\
		return;\
	}

void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *,size_t sz,void *addr)
{
	InvalidateDeviceObjects();
	glGenBuffers(1, &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, sz, addr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	size=sz;
}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	SAFE_DELETE_BUFFER(ubo);
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int )
{
GL_ERROR_CHECK
	static int lastBindingIndex=21;
	if(lastBindingIndex>=GL_MAX_UNIFORM_BUFFER_BINDINGS)
		lastBindingIndex=1;
	bindingIndex=lastBindingIndex;
	lastBindingIndex++;
GL_ERROR_CHECK
	for(crossplatform::TechniqueMap::iterator i=effect->techniques.begin();i!=effect->techniques.end();i++)
	{
		const char *techname=i->first.c_str();
		crossplatform::EffectTechnique *tech=i->second;
		if(!tech)
			break;
		bool any=false;
		for(int j=0;j<tech->NumPasses();j++)
		{
	GL_ERROR_CHECK
			GLuint program=tech->passAsGLuint(j);
			GLint indexInShader;
	GL_ERROR_CHECK
			indexInShader=glGetUniformBlockIndex(program,name);
			if(indexInShader>=0)
			{
				any=true;
	GL_ERROR_CHECK
				glUniformBlockBinding(program,indexInShader,bindingIndex);
	GL_ERROR_CHECK
				glBindBufferBase(GL_UNIFORM_BUFFER,bindingIndex,ubo);
	GL_ERROR_CHECK
				glBindBufferRange(GL_UNIFORM_BUFFER,bindingIndex,ubo,0,size);	
	GL_ERROR_CHECK
			}
			else
				std::cerr<<"PlatformConstantBuffer::LinkToEffect did not find the buffer named "<<name<<" in pass "<<j<<" of "<<techname<<std::endl;
		}
		if(!any)
			std::cerr<<"PlatformConstantBuffer::LinkToEffect did not find the buffer named "<<name<<" in the technique "<<techname<<"."<<std::endl;
	}
}

void PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &,size_t size,void *addr)
{
GL_ERROR_CHECK
	glBindBuffer(GL_UNIFORM_BUFFER,ubo);
	glBufferSubData(GL_UNIFORM_BUFFER,0,size,addr);
	glBindBuffer(GL_UNIFORM_BUFFER,0);
	glBindBufferBase(GL_UNIFORM_BUFFER,bindingIndex,ubo);
GL_ERROR_CHECK
}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &)
{
}

int EffectTechnique::NumPasses() const
{
	return passes_by_index.size();
}

Effect::Effect(crossplatform::RenderPlatform *,const char *filename_utf8,const std::map<std::string,std::string> &defines)
	:current_texture_number(0)
{
	filename=filename_utf8;
	platform_effect		=(void*)opengl::CreateEffect(filename_utf8,defines);
	FillInTechniques();
}

Effect::~Effect()
{
}

// convert GL programs into techniques and passes.
void Effect::FillInTechniques()
{
	GLint e				=asGLint();
	if(e<0)
		return ;
	int nump			=glfxGetProgramCount(e);
	if(!nump)
		return;
	groups.clear();
	for(int i=0;i<nump;i++)
	{
		std::string name	=glfxGetProgramName(e,i);
		GLuint t			=glfxCompileProgram(e,name.c_str());
		if(!t)
		{
			opengl::printEffectLog(e);
			return;
		}
		// Now the name will determine what technique and pass it is.
		std::string groupname;
		std::string techname=name;
		std::string passname="main";
		int dotpos1=techname.find("::");
		if(dotpos1>=0)
		{
			groupname	=name.substr(0,dotpos1);
			techname	=name.substr(dotpos1+2,techname.length()-dotpos1-2);
		}
		int dotpos2=techname.find_last_of(".");
		if(dotpos2>=0)
		{
			passname	=techname.substr(dotpos2+1,techname.length()-dotpos2-1);
			techname	=techname.substr(0,dotpos2);
		}
		crossplatform::EffectTechnique *tech=NULL;
		if(groupname.size()>0)
		{
			if(groups.find(groupname)==groups.end())
			{
				groups[groupname]=new crossplatform::EffectTechniqueGroup;
			}
			crossplatform::EffectTechniqueGroup *group=groups[groupname];
			if(group->techniques.find(techname)!=group->techniques.end())
				tech=group->techniques[techname];
			else
			{
				tech								=new opengl::EffectTechnique; 
				int index							=(int)group->techniques.size();
				group->techniques[techname]			=tech;
				group->techniques_by_index[index]	=tech;
			}
			techname=(groupname+"::")+techname;
		}
		if(techniques.find(techname)!=techniques.end())
		{
			if(!tech)
				tech=techniques[techname];
			else
				techniques[techname]=tech;
		}
		else
		{
			if(!tech)
				tech						=new opengl::EffectTechnique; 
			techniques[techname]		=tech;
			int index					=(int)techniques_by_index.size();
			techniques_by_index[index]	=tech;
		}
		tech->passes_by_name[passname]	=(void*)t;
		int pass_idx					=tech->passes_by_index.size();
		tech->passes_by_index[pass_idx]	=(void*)t;
		tech->pass_indices[passname]	=pass_idx;
	}
}

crossplatform::EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	if(asGLint()==-1)
		return NULL;
	GLint e									=asGLint();
	GLuint t								=glfxCompileProgram(e,name);
	if(!t)
	{
		opengl::printEffectLog(e);
		return NULL;
	}
	crossplatform::EffectTechnique *tech	=new opengl::EffectTechnique;
	tech->platform_technique				=(void*)t;
	techniques[name]						=tech;
	// Now it needs to be in the techniques_by_index list.
	size_t index							=glfxGetProgramIndex(e,name);
	techniques_by_index[index]				=tech;
	return tech;
}

crossplatform::EffectTechnique *Effect::GetTechniqueByIndex(int index)
{
	if(techniques_by_index.find(index)!=techniques_by_index.end())
	{
		return techniques_by_index[index];
	}
	if(asGLint()==-1)
		return NULL;
	GLint e				=asGLint();
	if(index>=techniques.size())
		return NULL;
//	int nump			=glfxGetProgramCount(e);
	const char *name	=glfxGetProgramName(e,index);
	GLuint t			=glfxCompileProgram(e,name);
	if(!t)
	{
		opengl::printEffectLog(e);
		return NULL;
	}
	crossplatform::EffectTechnique *tech	=new opengl::EffectTechnique;
	techniques[name]						=tech;
	techniques_by_index[index]				=tech;
	tech->platform_technique				=(void*)t;
	return tech;
}

void Effect::SetUnorderedAccessView(const char *name,crossplatform::Texture *tex)
{
	SetTexture(name,tex);
}

void Effect::SetTexture(const char *name,crossplatform::Texture *tex)
{
	current_texture_number++;
    glActiveTexture(GL_TEXTURE0+current_texture_number);
	// Fall out silently if this texture is not set.
	if(!tex)
		return;
	if(!tex->AsGLuint())
		return;
	if(tex->GetDimension()==2)
		glBindTexture(GL_TEXTURE_2D,tex->AsGLuint());
	else if(tex->GetDimension()==3)
		glBindTexture(GL_TEXTURE_3D,tex->AsGLuint());
	else
		throw simul::base::RuntimeError("Unknown texture dimension!");
    glActiveTexture(GL_TEXTURE0+current_texture_number);
GL_ERROR_CHECK
	if(currentTechnique)
	{
		GLuint program	=currentTechnique->passAsGLuint(currentPass);
		GLint loc		=glGetUniformLocation(program,name);
GL_ERROR_CHECK
	CHECK_PARAM_EXISTS
		glUniform1i(loc,current_texture_number);
	}
	else
	{
GL_ERROR_CHECK
		for(crossplatform::TechniqueMap::iterator i=techniques.begin();i!=techniques.end();i++)
		{
GL_ERROR_CHECK
			for(int j=0;j<i->second->NumPasses();j++)
			{
				GLuint program	=i->second->passAsGLuint(j);
	GL_ERROR_CHECK
				GLint loc		=glGetUniformLocation(program,name);
	GL_ERROR_CHECK
				if(loc>=0)
				{
					glUseProgram(program);
					glUniform1i(loc,current_texture_number);
					glUseProgram(0);
				}
GL_ERROR_CHECK
			}
		}
	}
GL_ERROR_CHECK
}

void Effect::SetTexture(const char *name,crossplatform::Texture &t)
{
	SetTexture(name,&t);
}

void Effect::SetParameter	(const char *name	,float value)
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform1f(loc,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec2 value)
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform2fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec3 value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform3fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,vec4 value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform4fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetParameter	(const char *name	,int value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform1i(loc,value);
	GL_ERROR_CHECK
}

void Effect::SetVector		(const char *name	,const float *value)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	glUniform4fv(loc,1,value);
	GL_ERROR_CHECK
}

void Effect::SetMatrix		(const char *name	,const float *m)	
{
	CHECK_TECH_EXISTS
	GLint loc=glGetUniformLocation(currentTechnique->passAsGLuint(currentPass),name);
	CHECK_PARAM_EXISTS
	SIMUL_ASSERT_WARN(loc>=0,(std::string("Parameter not found in GL Effect: ")+name).c_str());
	glUniformMatrix4fv(loc,1,false,m);
	GL_ERROR_CHECK
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass)
{
	if(apply_count!=0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	GL_ERROR_CHECK
	apply_count++;
	currentTechnique		=effectTechnique;
	currentPass				=pass;
	CHECK_TECH_EXISTS
	glUseProgram(effectTechnique->passAsGLuint(pass));
	GL_ERROR_CHECK
	current_texture_number	=0;
	EffectTechnique *glEffectTechnique=(EffectTechnique*)effectTechnique;
	if(glEffectTechnique->passStates.find(currentPass)!=glEffectTechnique->passStates.end())
		glEffectTechnique->passStates[currentPass]->Apply();
}

void Effect::Apply(crossplatform::DeviceContext &,crossplatform::EffectTechnique *effectTechnique,const char *pass)
{
	if(apply_count!=0)
		SIMUL_BREAK("Effect::Apply without a corresponding Unapply!")
	GL_ERROR_CHECK
	apply_count++;
	currentTechnique		=effectTechnique;
	CHECK_TECH_EXISTS
	GLuint prog=effectTechnique->passAsGLuint(pass);
	for(EffectTechnique::PassIndexMap::iterator i=effectTechnique->passes_by_index.begin();
		i!=effectTechnique->passes_by_index.end();i++)
	{
		if(i->second==(void*)prog)
			currentPass				=i->first;
	}
	//currentPass=effectTechnique->passes_by_index.find((void*)prog);
	glUseProgram(prog);
	GL_ERROR_CHECK
	current_texture_number	=0;
	EffectTechnique *glEffectTechnique=(EffectTechnique*)effectTechnique;
	if(glEffectTechnique->passStates.find(currentPass)!=glEffectTechnique->passStates.end())
		glEffectTechnique->passStates[currentPass]->Apply();
}

void Effect::Unapply(crossplatform::DeviceContext &)
{
	glUseProgram(0);
	if(apply_count<=0)
		SIMUL_BREAK("Effect::Unapply without a corresponding Apply!")
	else if(apply_count>1)
		SIMUL_BREAK("Effect::Apply has been called too many times!")
	currentTechnique=NULL;
	apply_count--;
	current_texture_number	=0;
GL_ERROR_CHECK
}