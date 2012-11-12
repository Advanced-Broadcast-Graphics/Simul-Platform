#pragma once
#include "Simul/Platform/OpenGL/Export.h"


namespace simul
{
	namespace opengl
	{
		extern SIMUL_OPENGL_EXPORT void SetShaderPath(const char *path);
	}
}

extern SIMUL_OPENGL_EXPORT GLuint MakeProgram(const char *filename,const char *defines=0);
extern SIMUL_OPENGL_EXPORT GLuint MakeProgramWithGS(const char *filename,const char *defines=0);

extern SIMUL_OPENGL_EXPORT GLuint SetShaders(const char *vert_src,const char *frag_src);
extern SIMUL_OPENGL_EXPORT GLuint LoadPrograms(const char *vert_filename,const char *geom_filename,const char *frag_filename,const char *defines=0);

extern SIMUL_OPENGL_EXPORT GLuint SetProgram(GLuint prog,const char *shader_source,const char *defines);
extern SIMUL_OPENGL_EXPORT GLuint LoadShader(GLuint prog,const char *filename,const char *defines=0);

extern SIMUL_OPENGL_EXPORT void printProgramInfoLog(GLuint obj);
#ifdef SIMULWEATHER_X_PLANE
#ifdef _MSC_VER
	#define IBM 1
#else
	#define LIN 1
#endif
#include "XPLMGraphics.h"
#define glGenTextures(a,b) XPLMGenerateTextureNumbers((int*)b,(int)a)
#endif
