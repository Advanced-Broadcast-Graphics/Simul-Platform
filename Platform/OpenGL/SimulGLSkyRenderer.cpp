

#include "Simul/Base/Timer.h"
#include <stdio.h>
#include <math.h>

#include <GL/glew.h>

#include <fstream>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"

#include "SimulGLSkyRenderer.h"
#include "Simul/Sky/Sky.h"
#include "Simul/Sky/TextureGenerator.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Math/Pi.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Base/SmartPtr.h"
#include "LoadGLImage.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/OpenGL/GLSL/simul_earthshadow_uniforms.glsl"

void printShaderInfoLog(GLuint obj);
void printProgramInfoLog(GLuint obj);

#if 0//def _MSC_VER
GLenum sky_tex_format=GL_HALF_FLOAT_NV;
GLenum internal_format=GL_RGBA16F_ARB;
#else
GLenum sky_tex_format=GL_FLOAT;
GLenum internal_format=GL_RGBA32F_ARB;
#endif

static GLint faces[6][4] = {  /* Vertex indices for the 6 faces of a cube. */
  {0, 1, 2, 3}, {3, 2, 6, 7}, {7, 6, 5, 4},
  {4, 5, 1, 0}, {5, 6, 2, 1}, {7, 4, 0, 3} };
static GLfloat v[8][3];  /* Will be filled in with X,Y,Z vertexes. */

SimulGLSkyRenderer::SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk)
	:BaseSkyRenderer(sk)
	,campos_updated(false)
	,short_ptr(NULL)
	,loss_2d(0,0,GL_TEXTURE_2D)
	,inscatter_2d(0,0,GL_TEXTURE_2D)
	,skylight_2d(0,0,GL_TEXTURE_2D)

	,loss_texture(0)
	,insc_texture(0)
	,skyl_texture(0)

	,sky_program(0)
	,earthshadow_program(0)
	,current_program(0)
	,planet_program(0)
	,fade_3d_to_2d_program(0)
	,sun_program(0)
	,stars_program(0)
	,initialized(false)
{

/* Setup cube vertex data. */
	v[0][0] = v[1][0] = v[2][0] = v[3][0] = -100.f;
	v[4][0] = v[5][0] = v[6][0] = v[7][0] =  100.f;
	v[0][1] = v[1][1] = v[4][1] = v[5][1] = -100.f;
	v[2][1] = v[3][1] = v[6][1] = v[7][1] =  100.f;
	v[0][2] = v[3][2] = v[4][2] = v[7][2] =  100.f;
	v[1][2] = v[2][2] = v[5][2] = v[6][2] = -100.f;
	for(int i=0;i<3;i++)
	{
		loss_textures[i]=inscatter_textures[i]=skylight_textures[i]=0;
	}
//	skyKeyframer->SetFillTexturesAsBlocks(true);
	SetCameraPosition(0,0,skyKeyframer->GetAltitudeKM()*1000.f);
}

void SimulGLSkyRenderer::SetFadeTexSize(int width_num_distances,int height_num_elevations,int num_alts)
{
	// If not initialized we might not have a valid GL context:
	if(!initialized)
		return;
	if(numFadeDistances==width_num_distances&&numFadeElevations==height_num_elevations&&numAltitudes==num_alts)
		return;
	numFadeDistances=width_num_distances;
	numFadeElevations=height_num_elevations;
	numAltitudes=num_alts;
	CreateFadeTextures();
}

void SimulGLSkyRenderer::CreateFadeTextures()
{
	unsigned *fade_tex_data=new unsigned[numFadeDistances*numFadeElevations*numAltitudes*sizeof(float)];
	glGenTextures(3,loss_textures);
	ERROR_CHECK
	glGenTextures(3,inscatter_textures);
	ERROR_CHECK
	glGenTextures(3,skylight_textures);
	ERROR_CHECK
	for(int i=0;i<3;i++)
	{
		{
			glBindTexture(GL_TEXTURE_3D,loss_textures[i]);
	ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
			glBindTexture(GL_TEXTURE_3D,inscatter_textures[i]);
	ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
			glBindTexture(GL_TEXTURE_3D,skylight_textures[i]);
	ERROR_CHECK
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE);
			glTexImage3D(GL_TEXTURE_3D,0,internal_format,numAltitudes,numFadeElevations,numFadeDistances,0,GL_RGBA,sky_tex_format,fade_tex_data);
		}
	}
	ERROR_CHECK
	delete [] fade_tex_data;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);

	SAFE_DELETE_TEXTURE(loss_texture);
	SAFE_DELETE_TEXTURE(insc_texture);
	SAFE_DELETE_TEXTURE(skyl_texture);
	loss_texture=make2DTexture(numFadeDistances,numFadeElevations);
	insc_texture=make2DTexture(numFadeDistances,numFadeElevations);
	skyl_texture=make2DTexture(numFadeDistances,numFadeElevations);


	ERROR_CHECK
}

static simul::sky::float4 Lookup(void *context,FramebufferGL fb,float distance_texcoord,float elevation_texcoord)
{
	distance_texcoord*=(float)fb.GetWidth();
	int x=(int)(distance_texcoord);
	if(x<0)
		x=0;
	if(x>fb.GetWidth()-2)
		x=fb.GetWidth()-2;
	float x_interp=distance_texcoord-x;
	elevation_texcoord*=(float)fb.GetHeight();
	int  	y=(int)(elevation_texcoord);
	if(y<0)
		y=0;
	if(y>fb.GetWidth()-2)
		y=fb.GetWidth()-2;
	float y_interp=elevation_texcoord-y;
	// four floats per texel, four texels.
	simul::sky::float4 data[4];
	fb.Activate(context);
	glReadPixels(x,y,2,2,GL_RGBA,GL_FLOAT,(GLvoid*)data);
	fb.Deactivate(context);
	simul::sky::float4 bottom	=simul::sky::lerp(x_interp,data[0],data[1]);
	simul::sky::float4 top		=simul::sky::lerp(x_interp,data[2],data[3]);
	simul::sky::float4 ret		=simul::sky::lerp(y_interp,bottom,top);
	return ret;
}

const float *SimulGLSkyRenderer::GetFastLossLookup(void *context,float distance_texcoord,float elevation_texcoord)
{
	return Lookup(context,loss_2d,distance_texcoord,elevation_texcoord);
}
const float *SimulGLSkyRenderer::GetFastInscatterLookup(void *context,float distance_texcoord,float elevation_texcoord)
{
	return Lookup(context,inscatter_2d,distance_texcoord,elevation_texcoord);
}

// Here we blend the four 3D fade textures (distance x elevation x altitude at two keyframes, for loss and inscatter)
// into pair of 2D textures (distance x elevation), eliminating the viewing altitude and time factor.
bool SimulGLSkyRenderer::Render2DFades(void *context)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	FramebufferGL *fb[]={&loss_2d,&loss_2d,&loss_2d};
	GLuint target_textures[]={loss_texture,insc_texture,skyl_texture};
	GLuint *input_textures[]={loss_textures,inscatter_textures,skylight_textures};
	glUseProgram(fade_3d_to_2d_program);
	glUniform1f(skyInterp_fade,skyKeyframer->GetInterpolation());
	glUniform1f(altitudeTexCoord_fade,skyKeyframer->GetAltitudeTexCoord());
	glUniform1i(fadeTexture1_fade,0);
	glUniform1i(fadeTexture2_fade,1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,0,1.0,-1.0,1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	fb[0]->Activate(context);
	for(int i=0;i<3;i++)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][0]);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D,input_textures[i][1]);
		DrawQuad(0,0,1,1);
		// copy to target:
		{
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glEnable(GL_TEXTURE_2D);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,target_textures[i]);
			glCopyTexSubImage2D(GL_TEXTURE_2D,0
					,0,0
					,0,0,
 					numFadeDistances,numFadeElevations
					);
			 glDisable(GL_TEXTURE_2D);
		}
	}
	fb[0]->Deactivate(context);
	glUseProgram(NULL);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,NULL);
	glDisable(GL_TEXTURE_3D);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	return true;
}

bool SimulGLSkyRenderer::RenderFades(void *,int w,int h)
{
	int size=w/4;
	if(h/3<size)
		size=h/3;
	if(size<2)
		return false;
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(11,"RenderFades");
	static int main_viewport[]={0,0,1,1};
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,(double)main_viewport[2],(double)main_viewport[3],0,-1.0,1.0);
ERROR_CHECK
	glUseProgram(0);
	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,loss_texture);
	RenderTexture(8,8,size,size);
	glBindTexture(GL_TEXTURE_2D,insc_texture);
	RenderTexture(8,16+size,size,size);
	glBindTexture(GL_TEXTURE_2D,skyl_texture);
	RenderTexture(8,24+2*size,size,size);
	int x=16+size;
	int s=size/numAltitudes-4;
	for(int i=0;i<numAltitudes;i++)
	{
		float atc=(float)(numAltitudes-0.5f-i)/(float)(numAltitudes);
		glUseProgram(fade_3d_to_2d_program);
		GLint altitudeTexCoord_fade	=glGetUniformLocation(fade_3d_to_2d_program,"altitudeTexCoord");
		GLint skyInterp_fade		=glGetUniformLocation(fade_3d_to_2d_program,"skyInterp");
		GLint fadeTexture1_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture1");
		GLint fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");

		glUniform1f(skyInterp_fade,0);
		glUniform1f(altitudeTexCoord_fade,atc);
		glUniform1i(fadeTexture1_fade,0);
		glUniform1i(fadeTexture2_fade,0);
		
		glActiveTexture(GL_TEXTURE0);
		
		glBindTexture(GL_TEXTURE_3D,loss_textures[0]);
		RenderTexture(x+16+0*(s+8)	,i*(s+4)+8, s,s);
		glBindTexture(GL_TEXTURE_3D,loss_textures[1]);
		RenderTexture(x+16+1*(s+8)	,i*(s+4)+8, s,s);
		glBindTexture(GL_TEXTURE_3D,inscatter_textures[0]);
		RenderTexture(x+16+0*(s+8)	,i*(s+4)+16+size, s,s);
		glBindTexture(GL_TEXTURE_3D,inscatter_textures[1]);
		RenderTexture(x+16+1*(s+8)	,i*(s+4)+16+size, s,s);
		glBindTexture(GL_TEXTURE_3D,skylight_textures[0]);
		RenderTexture(x+16+0*(s+8)	,i*(s+4)+24+2*size, s,s);
		glBindTexture(GL_TEXTURE_3D,skylight_textures[1]);
		RenderTexture(x+16+1*(s+8)	,i*(s+4)+24+2*size, s,s);
	}

	glUseProgram(0);
ERROR_CHECK
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,NULL);
	glDisable(GL_TEXTURE_2D);
ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
ERROR_CHECK
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
ERROR_CHECK
return true;
}
GLint earthShadowUniformsBindingIndex=3;

void SimulGLSkyRenderer::UseProgram(GLuint p)
{
	if(p&&p!=current_program)
	{
		current_program=p;
		MieRayleighRatio_param			=glGetUniformLocation(current_program,"mieRayleighRatio");
		lightDirection_sky_param		=glGetUniformLocation(current_program,"lightDir");
		hazeEccentricity_param			=glGetUniformLocation(current_program,"hazeEccentricity");
		skyInterp_param					=glGetUniformLocation(current_program,"skyInterp");
		skyTexture1_param				=glGetUniformLocation(current_program,"inscTexture");
		skylightTexture_param			=glGetUniformLocation(current_program,"skylightTexture");
			
		altitudeTexCoord_param			=glGetUniformLocation(current_program,"altitudeTexCoord");
		
		earthShadowUniforms				=glGetUniformBlockIndex(current_program, "EarthShadowUniforms");
ERROR_CHECK
		// If that block IS in the shader program, then BIND it to the relevant UBO.
		if(earthShadowUniforms>=0)
		{
			glUniformBlockBinding(current_program,earthShadowUniforms,earthShadowUniformsBindingIndex);
			glBindBufferRange(GL_UNIFORM_BUFFER,earthShadowUniformsBindingIndex,earthShadowUniformsUBO, 0, sizeof(EarthShadowUniforms));	
		}
		cloudOrigin					=glGetUniformLocation(current_program,"cloudOrigin");
		cloudScale					=glGetUniformLocation(current_program,"cloudScale");
		maxDistance					=glGetUniformLocation(current_program,"maxDistance");
		viewPosition				=glGetUniformLocation(current_program,"viewPosition");
		overcast_param				=glGetUniformLocation(current_program,"overcast");
		printProgramInfoLog(current_program);
	}
	glUseProgram(p);
}

bool SimulGLSkyRenderer::Render(void *context,bool blend)
{
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	EnsureTexturesAreUpToDate();
	simul::sky::float4 cam_dir;
	CalcCameraPosition(cam_pos,cam_dir);
	SetCameraPosition(cam_pos.x,cam_pos.y,cam_pos.z);
	static simul::base::Timer timer;
	timer.StartTime();
	Render2DFades(context);
	timer.FinishTime();
	texture_update_time=timer.Time;
	timer.StartTime();
ERROR_CHECK
	//if(!blend)
	//{
		glClearColor(0,0.0,0,1);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	//}
	simul::sky::float4 ratio=skyKeyframer->GetMieRayleighRatio();
	simul::sky::float4 light_dir=skyKeyframer->GetDirectionToLight(skyKeyframer->GetAltitudeKM());
	simul::sky::float4 sun_dir=skyKeyframer->GetDirectionToSun();
ERROR_CHECK
    glDisable(GL_DEPTH_TEST);
// We normally BLEND the sky because there may be hi-res things behind it like planets.
	if(blend)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
	}
	else
		glDisable(GL_BLEND);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

	campos_updated=true;
	glTranslatef(cam_pos[0],cam_pos[1],cam_pos[2]);
	simul::sky::EarthShadow e=skyKeyframer->GetEarthShadow(
								skyKeyframer->GetAltitudeKM()
								,skyKeyframer->GetDirectionToSun());

	//if(e.enable)
	{
		EarthShadowUniforms u;
		u.earthShadowNormal	=e.normal;
		u.radiusOnCylinder	=e.radius_on_cylinder;
		u.maxFadeDistance	=skyKeyframer->GetMaxDistanceKm()/skyKeyframer->GetSkyInterface()->GetPlanetRadius();
		u.terminatorCosine	=e.terminator_cosine;
		u.sunDir			=sun_dir;
		glBindBuffer(GL_UNIFORM_BUFFER, earthShadowUniformsUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(EarthShadowUniforms), &u);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	if(e.enable)
		UseProgram(earthshadow_program);
	else
		UseProgram(sky_program);
ERROR_CHECK
	//if(e.enable)
		glBindBufferBase(GL_UNIFORM_BUFFER,earthShadowUniformsBindingIndex,earthShadowUniformsUBO);
ERROR_CHECK

	glUniform1i(skyTexture1_param,0);
	
ERROR_CHECK
	//glUniform1f(altitudeTexCoord_param,skyKeyframer->GetAltitudeTexCoord());
	glUniform3f(MieRayleighRatio_param,ratio.x,ratio.y,ratio.z);
	glUniform1f(hazeEccentricity_param,skyKeyframer->GetMieEccentricity());
	//glUniform1f(skyInterp_param,skyKeyframer->GetInterpolation());
	glUniform3f(lightDirection_sky_param,light_dir.x,light_dir.y,light_dir.z);
	
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,insc_texture);
	glUniform1i(skylightTexture_param,1);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,skyl_texture);
	//setParameter3(cloudOrigin,cloud_origin);
	//setParameter3(cloudScale,cloud_scale);
	//setParameter(maxDistance,skyKeyframer->GetMaxDistanceKm()*1000.f);
	//setParameter(overcast_param,overcast);
	//setParameter3(viewPosition,cam_pos);
ERROR_CHECK
	for(int i=0;i<6;i++)
	{
		glBegin(GL_QUADS);
		glVertex3fv(&v[faces[i][0]][0]);
		glVertex3fv(&v[faces[i][1]][0]);
		glVertex3fv(&v[faces[i][2]][0]);
		glVertex3fv(&v[faces[i][3]][0]);
		glEnd();
	}
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,NULL);
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,NULL);
	
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D,NULL);

	glUseProgram(NULL);
	ERROR_CHECK
	timer.FinishTime();
	render_time=timer.Time;
	return true;
}

bool SimulGLSkyRenderer::RenderPointStars(void *)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	float sid[16];
	CalcCameraPosition(cam_pos);
	GetSiderealTransform(sid);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE,GL_SRC_ALPHA,GL_ONE);
	int current_num_stars=skyKeyframer->stars.GetNumStars();
	if(!star_vertices||current_num_stars!=num_stars)
	{
		num_stars=current_num_stars;
		delete [] star_vertices;
		star_vertices=new StarVertext[num_stars];
		static float d=100.f;
		{
			for(int i=0;i<num_stars;i++)
			{
				float ra=(float)skyKeyframer->stars.GetStar(i).ascension;
				float de=(float)skyKeyframer->stars.GetStar(i).declination;
				star_vertices[i].x= d*cos(de)*sin(ra);
				star_vertices[i].y= d*cos(de)*cos(ra);
				star_vertices[i].z= d*sin(de);
				star_vertices[i].b=(float)exp(-skyKeyframer->stars.GetStar(i).magnitude);
				star_vertices[i].c=1.f;
			}
		}
	}
	glUseProgram(stars_program);
	float sb=skyKeyframer->GetSkyInterface()->GetStarlight().x;
	float star_brightness=sb*skyKeyframer->GetStarBrightness();
	glUniform1f(starBrightness_param,star_brightness);
	float mat1[16],mat2[16];
	glGetFloatv(GL_MODELVIEW_MATRIX,mat1);

	glTranslatef(cam_pos.x,cam_pos.y,cam_pos.z);
	glGetFloatv(GL_MODELVIEW_MATRIX,mat2);

	glMultMatrixf(sid);
	glGetFloatv(GL_MODELVIEW_MATRIX,mat1);

	glBegin(GL_POINTS);
	for(int i=0;i<num_stars;i++)
	{
		StarVertext &V=star_vertices[i];
		glMultiTexCoord2f(GL_TEXTURE0,V.b,V.c);
		glVertex3f(V.x,V.y,V.z);
	}
	glEnd();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	return true;
}

void SimulGLSkyRenderer::RenderSun(void *,float exposure_hint)
{
	float alt_km=0.001f*cam_pos.z;
	simul::sky::float4 sun_dir(skyKeyframer->GetDirectionToSun());
	simul::sky::float4 sunlight=skyKeyframer->GetLocalIrradiance(alt_km);
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	sunlight*=pow(1.f-sun_occlusion,0.25f)*2700.f;
	// But to avoid artifacts like aliasing at the edges, we will rescale the colour itself
	// to the range [0,1], and store a brightness multiplier in the alpha channel!
	sunlight.w=1.f;
	float max_bright=std::max(std::max(sunlight.x,sunlight.y),sunlight.z);
	
	float maxout_brightness=2.f/exposure_hint;
	if(maxout_brightness>1e6f)
		maxout_brightness=1e6f;
	if(maxout_brightness<1e-6f)
		maxout_brightness=1e-6f;
	if(max_bright>maxout_brightness)
	{
		sunlight*=maxout_brightness/max_bright;
		sunlight.w=max_bright;
	}
	glUseProgram(sun_program);
	ERROR_CHECK
	glUniform4f(sunlight_param,sunlight.x,sunlight.y,sunlight.z,sunlight.w);
	ERROR_CHECK
	//if(y_vertical)
	//	std::swap(sun_dir.y,sun_dir.z);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE);
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	RenderAngledQuad(sun_dir,sun_angular_size);
	glUseProgram(0);
}

bool SimulGLSkyRenderer::RenderPlanet(void *,void* tex,float planet_angular_size,const float *dir,const float *colr,bool do_lighting)
{
		ERROR_CHECK
	CalcCameraPosition(cam_pos);
	float alt_km=0.001f*cam_pos.z;
	if(do_lighting)
	{
	}
	else
	{
	}

	simul::sky::float4 original_irradiance=skyKeyframer->GetSkyInterface()->GetSunIrradiance();

	simul::sky::float4 planet_dir4=dir;
	planet_dir4/=simul::sky::length(planet_dir4);

	simul::sky::float4 planet_colour(colr[0],colr[1],colr[2],1.f);
	float planet_elevation=asin(planet_dir4.z);
	planet_colour*=skyKeyframer->GetIsotropicColourLossFactor(alt_km,planet_elevation,0,1e10f);

	glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,(GLuint)((long int)tex));
	glUseProgram(planet_program);
		ERROR_CHECK
	glUniform3f(planetColour_param,planet_colour.x,planet_colour.y,planet_colour.z);
		ERROR_CHECK
	glUniform1i(planetTexture_param,0);
	// Transform light into planet space
	float Yaw=atan2(dir[0],dir[1]);
	float Pitch=-asin(dir[2]);
	simul::math::Vector3 sun_dir=skyKeyframer->GetDirectionToSun();
	simul::math::Vector3 sun2;
	{
		simul::geometry::SimulOrientation or;
		or.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		or.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		simul::math::Matrix4x4 inv_world;
		or.T4.Inverse(inv_world);
		simul::math::Multiply3(sun2,sun_dir,inv_world);
	}
		ERROR_CHECK
	glUniform3f(planetLightDir_param,sun2.x,sun2.y,sun2.z);
		ERROR_CHECK
	glUniform1i(planetTexture_param,0);

		ERROR_CHECK
	glEnable(GL_BLEND);
		ERROR_CHECK
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		ERROR_CHECK
	glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
		ERROR_CHECK
	glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		ERROR_CHECK

	bool res=RenderAngledQuad(dir,planet_angular_size);
	glUseProgram(NULL);
	return res;
}

void SimulGLSkyRenderer::Get2DLossAndInscatterTextures(void* *l1,void* *i1,void * *s)
{
	*l1=(void*)loss_texture;
	*i1=(void*)insc_texture;
	*s=(void*)skyl_texture;
}

void SimulGLSkyRenderer::FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d
	,const float *loss_float4_array,const float *inscatter_float4_array,
								const float *skylight_float4_array)
{
	if(!initialized)
		return;
	GLenum target=GL_TEXTURE_3D;
	glBindTexture(target,loss_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)loss_float4_array);
		ERROR_CHECK
	glBindTexture(target,inscatter_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)inscatter_float4_array);
		ERROR_CHECK
	glBindTexture(target,skylight_textures[texture_index]);
		ERROR_CHECK
	glTexSubImage3D(GL_TEXTURE_3D,0,x,y,z,w,l,d,GL_RGBA,sky_tex_format,(void*)skylight_float4_array);
		ERROR_CHECK
	glBindTexture(target,NULL);
		ERROR_CHECK
}

void SimulGLSkyRenderer::EnsureTexturesAreUpToDate()
{
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
	for(int i=0;i<3;i++)
	{
		simul::sky::BaseKeyframer::seq_texture_iterator &ft=fade_texture_iterator[i];
		simul::sky::BaseKeyframer::block_texture_fill t;
		while((t=skyKeyframer->GetBlockFadeTextureFill(i,ft)).w!=0)
		{
			FillFadeTextureBlocks(i,t.x,t.y,t.z,t.w,t.l,t.d,(const float*)t.float_array_1,(const float*)t.float_array_2,(const float*)t.float_array_3);
		}
	}
}

void SimulGLSkyRenderer::EnsureTextureCycle()
{
	int cyc=(skyKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(loss_textures[0],loss_textures[1]);
		std::swap(loss_textures[1],loss_textures[2]);
		std::swap(inscatter_textures[0],inscatter_textures[1]);
		std::swap(inscatter_textures[1],inscatter_textures[2]);
		std::swap(skylight_textures[0],skylight_textures[1]);
		std::swap(skylight_textures[1],skylight_textures[2]);
		std::swap(fade_texture_iterator[0],fade_texture_iterator[1]);
		std::swap(fade_texture_iterator[1],fade_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
		for(int i=0;i<3;i++)
			fade_texture_iterator[i].texture_index=i;
	}
}

void SimulGLSkyRenderer::ReloadTextures()
{
	moon_texture=(void*)LoadGLImage(skyKeyframer->GetMoonTexture().c_str(),GL_CLAMP_TO_EDGE);
	SetPlanetImage(moon_index,moon_texture);
}

void SimulGLSkyRenderer::RecompileShaders()
{
	current_program=0;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(earthshadow_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	SAFE_DELETE_PROGRAM(sun_program);
	SAFE_DELETE_PROGRAM(stars_program);
ERROR_CHECK
	sky_program						=MakeProgram("simul_sky");
	printProgramInfoLog(sky_program);
	earthshadow_program				=MakeProgram("simul_sky.vert",NULL,"simul_earthshadow_sky.frag");
	printProgramInfoLog(earthshadow_program);
ERROR_CHECK
	sun_program						=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	printProgramInfoLog(sun_program);
	stars_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_stars.frag");
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
	printProgramInfoLog(stars_program);
ERROR_CHECK
	sun_program						=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_sun.frag");
	sunlight_param					=glGetUniformLocation(sun_program,"sunlight");
	printProgramInfoLog(sun_program);
	stars_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_stars.frag");
	starBrightness_param			=glGetUniformLocation(stars_program,"starBrightness");
	printProgramInfoLog(stars_program);
ERROR_CHECK
	planet_program					=MakeProgram("simul_sun_planet_flare.vert",NULL,"simul_planet.frag");
ERROR_CHECK
	planetTexture_param				=glGetUniformLocation(planet_program,"planetTexture");
	planetColour_param				=glGetUniformLocation(planet_program,"colour");
	planetLightDir_param			=glGetUniformLocation(planet_program,"lightDir");
	printProgramInfoLog(planet_program);
ERROR_CHECK
	fade_3d_to_2d_program			=MakeProgram("simul_fade_3d_to_2d");
	
	altitudeTexCoord_fade	=glGetUniformLocation(fade_3d_to_2d_program,"altitudeTexCoord");
	skyInterp_fade		=glGetUniformLocation(fade_3d_to_2d_program,"skyInterp");
	fadeTexture1_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture1");
	fadeTexture2_fade		=glGetUniformLocation(fade_3d_to_2d_program,"fadeTexture2");

	glGenBuffers(1, &earthShadowUniformsUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, earthShadowUniformsUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(EarthShadowUniforms), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void SimulGLSkyRenderer::RestoreDeviceObjects(void*)
{
ERROR_CHECK
	initialized=true;
	loss_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	inscatter_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	skylight_2d.SetWidthAndHeight(numFadeDistances,numFadeElevations);
	loss_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	inscatter_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	skylight_2d.InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	
	SAFE_DELETE_TEXTURE(loss_texture);
	SAFE_DELETE_TEXTURE(insc_texture);
	SAFE_DELETE_TEXTURE(skyl_texture);
	loss_texture=make2DTexture(numFadeDistances,numFadeElevations);
	insc_texture=make2DTexture(numFadeDistances,numFadeElevations);
	skyl_texture=make2DTexture(numFadeDistances,numFadeElevations);

ERROR_CHECK
	RecompileShaders();
	glUseProgram(NULL);
ERROR_CHECK
	ReloadTextures();
	ClearIterators();
}

void SimulGLSkyRenderer::InvalidateDeviceObjects()
{
	initialized=false;
	SAFE_DELETE_PROGRAM(sky_program);
	SAFE_DELETE_PROGRAM(earthshadow_program);
	SAFE_DELETE_PROGRAM(planet_program);
	SAFE_DELETE_PROGRAM(fade_3d_to_2d_program);
	glDeleteTextures(3,loss_textures);
	glDeleteTextures(3,inscatter_textures);
	glDeleteTextures(3,skylight_textures);
	SAFE_DELETE_TEXTURE(loss_texture);
	SAFE_DELETE_TEXTURE(insc_texture);
	SAFE_DELETE_TEXTURE(skyl_texture);
}

SimulGLSkyRenderer::~SimulGLSkyRenderer()
{
	InvalidateDeviceObjects();
}

void SimulGLSkyRenderer::DrawLines(void *,Vertext *lines,int vertex_count,bool strip)
{
	::DrawLines((VertexXyzRgba*)lines,vertex_count,strip);
}

void SimulGLSkyRenderer::PrintAt3dPos(void *,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	::PrintAt3dPos(p,text,colr,offsetx,offsety);
}

const char *SimulGLSkyRenderer::GetDebugText()
{
	simul::sky::EarthShadow e=skyKeyframer->GetEarthShadow(
								skyKeyframer->GetAltitudeKM()
								,skyKeyframer->GetDirectionToSun());
	static char txt[400];
	sprintf_s(txt,400,"e.normal (%4.4g, %4.4g, %4.4g) r-1: %4.4g, cos: %4.4g, illum alt: %4.4g",e.normal.x,e.normal.y,e.normal.z
		,e.radius_on_cylinder-1.f,e.terminator_cosine,e.illumination_altitude);
	return txt;
}