#ifndef SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#define SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#include <set>
#include <map>
#include <string>
#include <vector>
#include "Export.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Base/MemoryInterface.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Topology.h"
#include "Simul/Platform/CrossPlatform/SL/Cppsl.hs"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/debug_constants.sl"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif
struct ID3D11Device;
struct VertexXyzRgba
{
	float x,y,z;
	float r,g,b,a;
};
namespace simul
{
	/// The namespace and library for cross-platform base classes, which abstract rendering functionality.
	namespace crossplatform
	{
		class Material;
		class Effect;
		class EffectTechnique;
		class TextRenderer;
		struct EffectDefineOptions;
		class Light;
		class Texture;
		class BaseFramebuffer;
		class SamplerState;
		class Mesh;
		class PlatformConstantBuffer;
		class PlatformStructuredBuffer;
		class Buffer;
		class Layout;
		struct RenderState;
		struct RenderStateDesc;
		struct DeviceContext;
		struct LayoutDesc;
		struct SamplerStateDesc;
		struct PhysicalLightRenderData;
		struct Query;
		/// A crossplatform viewport structure.
		struct Viewport
		{
			int x,y;
			int w,h;
			float znear,zfar;
		};
		/// Given a viewport struct and a texture, get the texture coordinates that viewport represents within the texture.
		vec4 SIMUL_CROSSPLATFORM_EXPORT ViewportToTexCoordsXYWH(const Viewport *vi,const Texture *t);
		/// A base class for API-specific rendering.

		/*! RenderPlatform is an interface that allows Simul's rendering functions to be developed
			in a cross-platform manner. By abstracting the common functionality of the different graphics API's
			into an interface, we can write render code that need not know which API is being used. It is possible
			to create platform-specific objects like /link CreateTexture textures/endlink, /link CreateEffect effects/endlink
			and /link CreateBuffer buffers/endlink

			Be sure to make the following calls at the appropriate places:
			RestoreDeviceObjects(), InvalidateDeviceObjects(), RecompileShaders()
			*/
		class SIMUL_CROSSPLATFORM_EXPORT RenderPlatform
		{
		public:
			RenderPlatform(simul::base::MemoryInterface*m=NULL);
			struct Vertext
			{
				vec3 pos;
				vec4 colour;
			};
			virtual ID3D11Device *AsD3D11Device();
			//! Call this once, when the 3D graphics device has been initialized, and pass the API-specific device pointer/identifier.
			virtual void RestoreDeviceObjects(void*);
			//! Call this once, when the 3d graphics device object is being shut down.
			virtual void InvalidateDeviceObjects();
			//! Optional - call this to recompile the standard shaders.
			virtual void RecompileShaders	();
			//! Gets an object containing immediate-context API-specific values.
			DeviceContext &GetImmediateContext();
			virtual void PushTexturePath	(const char *pathUtf8);
			virtual void PopTexturePath		();
			virtual void StartRender		(DeviceContext &deviceContext)=0;
			virtual void EndRender			(DeviceContext &deviceContext)=0;
			virtual void IntializeLightingEnvironment(const float pAmbientLight[3])		=0;
			virtual void DispatchCompute	(DeviceContext &deviceContext,int w,int l,int d)=0;
			virtual void ApplyShaderPass	(DeviceContext &deviceContext,Effect *,EffectTechnique *,int)=0;
			virtual void Draw				(DeviceContext &deviceContext,int num_verts,int start_vert)=0;
			virtual void DrawIndexed		(DeviceContext &deviceContext,int num_indices,int start_index=0,int base_vertex=0)=0;
			virtual void DrawMarker			(DeviceContext &deviceContext,const double *matrix)			=0;
			virtual void DrawLine			(DeviceContext &deviceContext,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)=0;
			virtual void DrawCrossHair		(DeviceContext &deviceContext,const double *pGlobalPosition)	=0;
			virtual void DrawCamera			(DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll)=0;
			virtual void DrawLineLoop		(DeviceContext &deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4])=0;

			virtual void DrawTexture		(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f,bool blend=false)=0;
			virtual void DrawDepth			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v=NULL)=0;
			// Draw an onscreen quad without passing vertex positions, but using the "rect" constant from the shader to pass the position and extent of the quad.
			virtual void DrawQuad			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique,const char *pass=NULL)=0;
			virtual void DrawQuad			(DeviceContext &deviceContext)=0;

			virtual void Print				(DeviceContext &deviceContext,int x,int y,const char *text,const float* colr=NULL,const float* bkg=NULL);
			virtual void DrawLines			(DeviceContext &deviceContext,Vertext *lines,int count,bool strip=false,bool test_depth=false,bool view_centred=false)		=0;
			virtual void Draw2dLines		(DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)		=0;
			virtual void DrawCircle			(DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false)		=0;
			virtual void DrawCubemap		(DeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float exposure,float gamma);
			virtual void PrintAt3dPos		(DeviceContext &deviceContext,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0,bool centred=false)		=0;
			virtual void SetModelMatrix		(crossplatform::DeviceContext &deviceContext,const double *mat,const crossplatform::PhysicalLightRenderData &physicalLightRenderData)	=0;
			virtual void					ApplyDefaultMaterial			()	=0;
			/// Create a platform-specific material instance.
			virtual Material				*CreateMaterial					()	=0;
			/// Create a platform-specific mesh instance.
			virtual Mesh					*CreateMesh						()	=0;
			/// Create a platform-specific light instance.
			virtual Light					*CreateLight					()	=0;
			/// Create a platform-specific texture instance.
			virtual Texture					*CreateTexture					(const char *lFileNameUtf8=NULL)	=0;
			/// Create a platform-specific framebuffer instance - i.e. an optional colour and an optional depth rendertarget.
			virtual BaseFramebuffer			*CreateFramebuffer				()	=0;
			/// Create a platform-specific sampler state instance.
			virtual SamplerState			*CreateSamplerState				(SamplerStateDesc *)	=0;
			/// Create a platform-specific effect instance.
			Effect							*CreateEffect					(const char *filename_utf8);
			/// Create a platform-specific effect instance.
			virtual Effect					*CreateEffect					(const char *filename_utf8,const std::map<std::string,std::string> &defines)=0;
			/// Create a platform-specific constant buffer instance. This is not usually used directly, instead, create a
			/// simul::crossplatform::ConstantBuffer, and pass this RenderPlatform's pointer to it in RestoreDeviceObjects().
			virtual PlatformConstantBuffer	*CreatePlatformConstantBuffer	()	=0;
			/// Create a platform-specific structured buffer instance. This is not usually used directly, instead, create a
			/// simul::crossplatform::StructuredBuffer, and pass this RenderPlatform's pointer to it in RestoreDeviceObjects().
			virtual PlatformStructuredBuffer	*CreatePlatformStructuredBuffer	()	=0;
			/// Create a platform-specific buffer instance - e.g. vertex buffers, index buffers etc.
			virtual Buffer					*CreateBuffer					()	=0;
			/// Create a platform-specific layout instance based on the given layout description \em layoutDesc and buffer \em buffer.
			virtual Layout					*CreateLayout					(int num_elements,const LayoutDesc *layoutDesc)	=0;
			/// Create a platform-specific RenderState object - e.g. a Blend state, Depth state, etc.
			virtual RenderState				*CreateRenderState				(const RenderStateDesc &desc)=0;
			/// Create an API-specific query object, e.g. for occlusion or timing tests.
			virtual Query					*CreateQuery					(QueryType q)=0;
			// API stuff: these are the main API-call replacements, corresponding to devicecontext calls in DX11:
			/// Activate the specifided vertex buffers in preparation for rendering.
			virtual void					SetVertexBuffers				(DeviceContext &deviceContext,int slot,int num_buffers,Buffer **buffers,const crossplatform::Layout *layout)=0;
			/// Graphics hardware can write to vertex buffers using vertex and geometry shaders; use this function to set the target buffer.
			virtual void					SetStreamOutTarget				(DeviceContext &deviceContext,Buffer *buffer)=0;
			/// Make the specified rendertargets and optional depth target active.
			virtual void					ActivateRenderTargets			(DeviceContext &deviceContext,int num,Texture **targs,Texture *depth)=0;
			virtual void					SetViewports(DeviceContext &deviceContext,int num,Viewport *vps)=0;
			/// Get the viewport at the given index.
			virtual Viewport				GetViewport(DeviceContext &deviceContext,int index)=0;
			/// Activate the specified index buffer in preparation for rendering.
			virtual void					SetIndexBuffer					(DeviceContext &deviceContext,Buffer *buffer)=0;
			//! Set the topology for following draw calls, e.g. TRIANGLELIST etc.
			virtual void					SetTopology						(DeviceContext &deviceContext,Topology t)=0;
			/// This function is called to ensure that the named shader is compiled with all the possible combinations of \#define's given in \em options.
			virtual void					EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<EffectDefineOptions> &options);
			/// Called to store the render state - blending, depth check, etc. - for later retrieval with RestoreRenderState.
			/// Some platforms may not support this.
			virtual void					StoreRenderState				(DeviceContext &deviceContext)=0;
			/// Called to restore the render state previously stored with StoreRenderState. There must be exactly one call of RestoreRenderState
			/// for each StoreRenderState call, and they are not expected to be nested.
			virtual void					RestoreRenderState				(DeviceContext &deviceContext)=0;
			/// Apply the RenderState to the device context - e.g. blend state, depth masking etc.
			virtual void					SetRenderState					(DeviceContext &deviceContext,const RenderState *s)=0;
			/// Apply a standard renderstate - e.g. opaque blending
			virtual void					SetStandardRenderState			(DeviceContext &deviceContext,StandardRenderState s);
			//! Store the current rendertargets and viewports at the top of the stack
			virtual void					PushRenderTargets(DeviceContext &deviceContext)=0;
			//! Restore rendertargets and viewports from the top of the stack.
			virtual void					PopRenderTargets(DeviceContext &deviceContext)=0;
			//! Resolve a MSAA texture to a normal texture.
			virtual void					Resolve(DeviceContext &deviceContext,Texture *destination,Texture *source)=0;
			//! Save a texture to disk.
			virtual void					SaveTexture(Texture *texture,const char *lFileNameUtf8)=0;
			//! This was introduced because Unity's deferred renderer flips the image vertically sometime after we render.
			bool mirrorY, mirrorY2, mirrorYText;
			crossplatform::Effect *solidEffect;
			std::set<crossplatform::Material*> materials;
			std::vector<std::string> GetTexturePathsUtf8();
			void SetShaderBinaryPathUtf8(const char *path_utf8)
			{
				shaderBinaryPathUtf8=path_utf8;
			}
			const char *GetShaderBinaryPathUtf8()
			{
				return shaderBinaryPathUtf8.c_str();
			}
			simul::base::MemoryInterface *GetMemoryInterface()
			{
				return memoryInterface;
			}
		protected:
			simul::base::MemoryInterface *memoryInterface;
			std::vector<std::string> shaderPathsUtf8;
			std::vector<std::string> texturePathsUtf8;
			std::string shaderBinaryPathUtf8;
		protected:
			DeviceContext					immediateContext;
		private:
			TextRenderer					*textRenderer;
			std::map<StandardRenderState,RenderState*> standardRenderStates;
			void							EnsureEffectIsBuiltPartialSpec	(const char *filename_utf8,const std::vector<EffectDefineOptions> &options,const std::map<std::string,std::string> &defines);
		};

		/// Draw a horizontal grid, centred around the camera, at z=0.
		///
		/// \param [in,out]	deviceContext	Context for the device.
		/// \param [in]	square_size	Spacing between lines - in whatever units the renderer is working in.
		/// \param [in]	brightness 	Brightness of the lines.
		/// \param [in]	numLines	Number of gridlines to draw.
		extern SIMUL_CROSSPLATFORM_EXPORT void DrawGrid(crossplatform::DeviceContext &deviceContext,float square_size,float brightness,int numLines);
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif
