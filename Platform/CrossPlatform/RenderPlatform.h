#ifndef SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#define SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#include <set>
#include <map>
#include <string>
#include "Export.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
struct ID3D11Device;
namespace simul
{
	namespace crossplatform
	{
		class Material;
		class Effect;
		class EffectTechnique;
		class Light;
		class Texture;
		class Mesh;
		class PlatformConstantBuffer;
		class Buffer;
		class Layout;
		struct DeviceContext;
		struct LayoutDesc;
		/// Base class for API-specific rendering.
		/// Be sure to make the following calls at the appropriate place: RestoreDeviceObjects(), InvalidateDeviceObjects(), RecompileShaders(), SetReverseDepth()
		class RenderPlatform
		{
		public:
			struct Vertext
			{
				vec3 pos;
				vec4 colour;
			};
			virtual ID3D11Device *AsD3D11Device()=0;
			virtual void PushTexturePath	(const char *pathUtf8)=0;
			virtual void PopTexturePath		()=0;
			virtual void StartRender		()=0;
			virtual void EndRender			()=0;
			virtual void SetReverseDepth	(bool)=0;
			virtual void IntializeLightingEnvironment(const float pAmbientLight[3])		=0;

			virtual void ApplyShaderPass	(DeviceContext &deviceContext,Effect *,EffectTechnique *,int)=0;
			virtual void Draw				(DeviceContext &deviceContext,int num_verts,int start_vert)=0;
			virtual void DrawMarker			(void *context,const double *matrix)			=0;
			virtual void DrawLine			(void *context,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)=0;
			virtual void DrawCrossHair		(void *context,const double *pGlobalPosition)	=0;
			virtual void DrawCamera			(void *context,const double *pGlobalPosition, double pRoll)=0;
			virtual void DrawLineLoop		(void *context,const double *mat,int num,const double *vertexArray,const float colr[4])=0;

			virtual void DrawTexture		(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f)=0;
			virtual void DrawDepth			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const float *proj)=0;
			// Draw an onscreen quad without passing vertex positions, but using the "rect" constant from the shader to pass the position and extent of the quad.
			virtual void DrawQuad			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique)=0;
			virtual void DrawQuad			(DeviceContext &deviceContext)=0;

			virtual void Print				(DeviceContext &deviceContext,int x,int y,const char *text)							=0;
			virtual void DrawLines			(DeviceContext &deviceContext,Vertext *lines,int count,bool strip=false)		=0;
			virtual void Draw2dLines		(DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)		=0;
			virtual void DrawCircle			(DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false)		=0;
			virtual void PrintAt3dPos		(void *context,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0)		=0;
			virtual void					SetModelMatrix					(crossplatform::DeviceContext &deviceContext,const double *mat)	=0;
			virtual void					ApplyDefaultMaterial			()	=0;
			virtual Material				*CreateMaterial					()	=0;
			virtual Mesh					*CreateMesh						()	=0;
			virtual Light					*CreateLight					()	=0;
			virtual Texture					*CreateTexture					(const char *lFileNameUtf8=NULL)	=0;
			virtual Effect					*CreateEffect					(const char *filename_utf8,const std::map<std::string,std::string> &defines)=0;
			virtual PlatformConstantBuffer	*CreatePlatformConstantBuffer	()	=0;
			virtual Buffer					*CreateBuffer					()	=0;
			virtual Layout					*CreateLayout					(int num_elements,LayoutDesc *)	=0;
			virtual void					*GetDevice						()	=0;
			virtual void					SetVertexBuffers				(DeviceContext &deviceContext,int slot,int num_buffers,Buffer **buffers)=0;
		};
	}
}
#endif