#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#ifndef _MSC_VER
#include <cstdio>
#include <cstring>
#else
#include <windows.h>
#endif
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
static std::string shaderPath;
static std::string last_filename;

using namespace simul;
using namespace opengl;

void printShaderInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

	glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 1)
    {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
		std::string info_log=infoLog;
		if(info_log.find("No errors")>=info_log.length())
		{
			std::cerr<<std::endl<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
		}
		else if(info_log.find("WARNING")<info_log.length())
			std::cout<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
		free(infoLog);
    }
}

void printProgramInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 1)
    {
        infoLog = (char *)malloc(infologLength);
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		std::string info_log=infoLog;
		if(info_log.find("No errors")>=info_log.length())
		{
			std::cerr<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
		}
		else if(info_log.find("WARNING")<info_log.length())
			std::cout<<last_filename.c_str()<<":\n"<<infoLog<<std::endl;
        free(infoLog);
    }
}

namespace simul
{
	namespace opengl
	{
		void SetShaderPath(const char *path)
		{
			shaderPath=path;
		}
	}
}

GLuint MakeProgram(const char *filename,const char *defines)
{
	char v[100];
	char f[100];
	sprintf_s(v,98,"%s.vert",filename);
	sprintf_s(f,98,"%s.frag",filename);
	return LoadPrograms(v,NULL,f,defines);
}
GLuint MakeProgramWithGS(const char *filename,const char *defines)
{
	char v[100];
	char f[100];
	char g[100];
	sprintf_s(v,98,"%s.vert",filename);
	sprintf_s(f,98,"%s.frag",filename);
	sprintf_s(g,98,"%s.geom",filename);
	return LoadPrograms(v,g,f,defines);
}

GLuint SetShaders(const char *vert_src,const char *frag_src)
{
	GLuint prog				=glCreateProgram();
	GLuint vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);
    vertex_shader			=SetProgram(vertex_shader,vert_src,"");
    fragment_shader			=SetProgram(fragment_shader,frag_src,"");
	glAttachShader(prog,vertex_shader);
	glAttachShader(prog,fragment_shader);
	glLinkProgram(prog);
	glUseProgram(prog);
	printProgramInfoLog(prog);
	return prog;
}

GLuint LoadPrograms(const char *vert_filename,const char *geom_filename,const char *frag_filename,const char *defines)
{
	GLuint prog						=glCreateProgram();
	GLuint vertex_shader			=glCreateShader(GL_VERTEX_SHADER);
    vertex_shader					=LoadShader(vertex_shader,vert_filename,defines);
	glAttachShader(prog,vertex_shader);
	if(geom_filename)
	{
		GLuint geometry_shader			=glCreateShader(GL_GEOMETRY_SHADER);
		geometry_shader					=LoadShader(geometry_shader,geom_filename,defines);
		glAttachShader(prog,geometry_shader);
	ERROR_CHECK
	}
	GLuint fragment_shader			=glCreateShader(GL_FRAGMENT_SHADER);
    fragment_shader					=LoadShader(fragment_shader,frag_filename,defines);
	glAttachShader(prog,fragment_shader);
	glLinkProgram(prog);
	glUseProgram(prog);
	printProgramInfoLog(prog);
	ERROR_CHECK
	return prog;
}

GLuint SetProgram(GLuint prog,const char *shader_source,const char *defines)
{
/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
	const int MAX_LINES=512;
	const int MAX_LINE_LENGTH=256;					// 255 + NULL terminator
	static char program[MAX_LINES*MAX_LINE_LENGTH];
	char *ptr=program;

	if(defines)
	{
		int len=strlen(defines);
		strcpy_s(ptr,MAX_LINES*MAX_LINE_LENGTH,defines);
		ptr[len]='\n';
		ptr+=len+1;
	}
	if(shader_source)
	{
		int len=strlen(shader_source);
		strcpy_s(ptr,MAX_LINES*MAX_LINE_LENGTH,shader_source);
		ptr[len]='\n';
		ptr+=len+1;
	}
	ptr[0]=0;
	const char *strings[1];
	strings[0]=program;
	int lenOfStrings[1];
	lenOfStrings[0]=strlen(strings[0]);
	glShaderSource(prog,1,strings,NULL);
    if(!prog)
		return 0;
	else
	{
		glCompileShader(prog);
	}
	printShaderInfoLog(prog);

	int result=1;
	glGetShaderiv(prog,GL_COMPILE_STATUS,&result);
	if(!result)
	{
		std::cerr<<"ERROR:\tShader failed to compile\n";
		DebugBreak();
	}
    return prog;
}

GLuint LoadShader(GLuint prog,const char *filename,const char *defines)
{
    std::string filePath=shaderPath;
	char last=0;
	if(filePath.length())
		filePath[filePath.length()-1];
	if(last!='/'&&last!='\\')
		filePath+="/";
	filePath+=filename;
	last_filename=filePath;
	std::ifstream ifs(filePath.c_str());
	if(!ifs.good())
	{
		std::cerr<<"\nERROR:\tShader file "<<filename<<" not found, exiting.\n";
		std::cerr<<"\n\t\tShader path is "<<shaderPath.c_str()<<", is this correct?\n";
		exit(1);
	}

/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
	const int MAX_LINES=512;
	const int MAX_LINE_LENGTH=256;   // 255 + NULL terminator
	static char shader_source[MAX_LINES*MAX_LINE_LENGTH];
	char *ptr=shader_source;
	while(!ifs.eof())
	{
		ifs.getline(ptr,MAX_LINE_LENGTH);
		int len=strlen(ptr);
		ptr[len]='\n';
		ptr+=len+1;
	}
	ifs.close();
	ptr[0]=0;
	prog=SetProgram(prog,shader_source,defines);
    if(!prog)
		std::cerr<<std::endl<<filePath.c_str()<<"(0): Error creating program "<<std::endl;

    return prog;
}
