#ifndef SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#define SIMUL_PLATFORM_CROSSPLATFORM_BASERENDERER_H
#include "Simul/Math/Matrix4x4.h"
#include "SL/CppSl.hs"

namespace simul
{
	namespace crossplatform
	{
/// How to interpret the depth texture.
		enum DepthTextureStyle
		{
			/// Depth textures are interpreted as representing the z-output of the projection matrix transformation.
			PROJECTION
			/// Depth textures are interpreted as representing a linear distance in the z-direction from the near clipping plane.
			,DISTANCE_FROM_NEAR_PLANE
		};
		/// A simple struct encapsulating a view and a projection matrix.
		struct ViewStruct
		{
			int view_id;							///< An id unique to each rendered view, but persistent across frames.
			math::Matrix4x4 view;					///< The view matrix. If considered as row-major, position information is in the 4th row.
			math::Matrix4x4 proj;					///< The projection matrix, row-major.
			DepthTextureStyle depthTextureStyle;	///< How to interpret any depth texture passed from outside.
		};
		/// Values that represent what pass to render, be it the near pass, the far, or both: far to render target 0, near to render target 1.
		enum NearFarPass
		{
			FAR_PASS
			,NEAR_PASS
			,BOTH_PASSES
			,EIGHT_PASSES
		};
		/// A structure to translate between buffers of different resolutions.
		struct MixedResolutionStruct
		{
			inline MixedResolutionStruct(int W,int H,int s,vec2 offs)
			{
				this->W=W;
				this->H=H;
				if (s > 1)
				{
					w = (W + s - 1) / s + 1;
					h = (H + s - 1) / s + 1;
					pixelOffset = offs;
				}
				else
				{
					w = W;
					h = H;
					pixelOffset = vec2(0,0);
				}
				downscale=s;
			}
			int W,H;
			int w,h;
			int downscale;
			/// This is the offset in higher-resolution pixels from the top-left of the lower-res buffer to the top-left of the higher-res buffer.
			vec2 pixelOffset;
			// The ratios that we multiply low-res texture coordinates by to get the hi-res equivalent.
			// These will be close to one, but not exact unless the hi-res buffer is an exact multiple of the low-res one.
			//float xratio,yratio;GetTransformLowResToHiRes();
			inline vec4 GetTransformLowResToHiRes() const
			{
				float A=(float)(w*downscale)/(float)W;
				float B=(float)(h*downscale)/(float)H;
				static float uu=0.5f;
				float X=(float)(-pixelOffset.x)/(float)W-uu*(1.f/w)*A;
				float Y=(float)(-pixelOffset.y)/(float)H-uu*(1.f/h)*B;
				return vec4(X,Y,A,B);
			}
			// X = -px/W - uu/w*a
			// U = X + A u
			// =>
			//		u= (U-X)/A
			//			=- X/A+(1/A)U
			//			= (-X/A) + (1/A) U
			//			= x + a U

			// where x=-X/A and a=1/A
			inline vec4 GetTransformHiResToLowRes() const
			{
				float a=(float)W/(float)(w*downscale);
				float b=(float)H/(float)(h*downscale);
				static float uu=0.5f;
				float x=(float)(pixelOffset.x)*a/(float)(W)+uu*(1.f/w);
				float y=(float)(pixelOffset.y)*b/(float)(H)+uu*(1.f/h);
				return vec4(x,y,a,b);//1.0f-1.0f/b
			}
		};
		//! The base class for renderers. Placeholder for now.
		class BaseRenderer
		{
		//protected:
		//	virtual scene::RenderPlatform *GetRenderPlatform()=0;
		};
	}
}
#endif