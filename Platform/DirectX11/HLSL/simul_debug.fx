#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/SL/depth.sl"
#include "../../CrossPlatform/SL/colour_packing.sl"

sampler2D imageTexture SIMUL_TEXTURE_REGISTER(0);
Texture2DMS<float4> imageTextureMS SIMUL_TEXTURE_REGISTER(1);
TextureCube cubeTexture SIMUL_TEXTURE_REGISTER(2);
Texture2D<uint> imageTextureUint SIMUL_TEXTURE_REGISTER(3);
Texture2D<uint2> imageTextureUint2 SIMUL_TEXTURE_REGISTER(4);
Texture2D<uint3> imageTextureUint3 SIMUL_TEXTURE_REGISTER(5);
Texture2D<uint4> imageTextureUint4 SIMUL_TEXTURE_REGISTER(6);

uniform_buffer DebugConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform mat4 worldViewProj;
	uniform int latitudes,longitudes;
	uniform float radius;
	uniform float multiplier;
	uniform float exposure;
	uniform vec4 colour;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec2 tanHalfFov;
	uniform float gamma;
};

cbuffer cbPerObject : register(b11)
{
	float4 rect;
};

struct a2v
{
    float3 position	: POSITION;
    float4 colour	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float4 colour		: TEXCOORD0;
};

posTexVertexOutput VS_Quad(idOnly id)
{
    return VS_ScreenQuad(id,rect);
}

v2f DebugVS(positionColourVertexInput IN)
{
	v2f OUT;
	vec3 pos		=IN.position.xyz;
    OUT.hPosition	=mul(worldViewProj,vec4(pos.xyz,1.0));
	OUT.colour		=IN.colour;
    return OUT;
}

v2f Debug2dVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition	=float4(rect.xy+rect.zw*IN.position.xy,0.0,1.0);
	OUT.colour		=IN.colour;
    return OUT;
}

v2f CircleVS(idOnly IN)
{
	v2f OUT;
	float angle		=2.0*3.1415926536*float(IN.vertex_id)/31.0;
	vec4 pos		=vec4(100.0*vec3(radius*vec2(cos(angle),sin(angle)),1.0),1.0);
    OUT.hPosition	=mul(worldViewProj,float4(pos.xyz,1.0));
	OUT.colour		=colour;
    return OUT;
}

v2f FilledCircleVS(idOnly IN)
{
	v2f OUT;
	int i=int(IN.vertex_id/2);
	int j=int(IN.vertex_id%2);
	float angle		=2.0*3.1415926536*float(IN.vertex_id)/31.0;
	vec4 pos		=vec4(100.0*vec3(radius*j*vec2(cos(angle),sin(angle)),1.0),1.0);
    OUT.hPosition	=mul(worldViewProj,float4(pos.xyz,1.0));
	OUT.colour		=colour;
    return OUT;
}

float4 DebugPS(v2f IN) : SV_TARGET
{
    return IN.colour;
}

vec4 PS_ShowTexture(posTexVertexOutput IN) : SV_TARGET
{
	vec4 res=multiplier*texture_nearest_lod(imageTexture,IN.texCoords.xy,0);
	//res.r=res.a;
	return res;
}


vec4 PS_CompactedTexture(posTexVertexOutput IN) : SV_TARGET
{
	uint2 dims;
	imageTextureUint4.GetDimensions(dims.x,dims.y);
	uint2 pos			=IN.texCoords*dims;
	uint4 lookup		=image_load(imageTextureUint4,pos);
	vec3 clr1=uint2_to_colour3(lookup.xy);
	vec3 clr2=uint2_to_colour3(lookup.zw);
#if 0
	vec3 clr=vec3(clr1.x,clr2.x,0.0);
#else
	vec3 clr=0.5*(clr2+clr1);
	//clr.r+=100.0*abs(clr1.x-clr2.x);
#endif
	vec3 res=multiplier*clr;
//	res.xy+=IN.texCoords.xy;
	return vec4(res,1.0);
}
vec4 PS_ShowTextureMS(posTexVertexOutput IN) : SV_TARGET
{
	uint2 dims;
	uint numSamples;
	imageTextureMS.GetDimensions(dims.x,dims.y,numSamples);
	uint2 pos	=uint2(IN.texCoords.xy*vec2(dims.xy));
	vec4 res	=multiplier*imageTextureMS.Load(pos,0);
	return res;
}

vec4 ShowDepthPS(posTexVertexOutput IN) : SV_TARGET
{
	vec4 depth		=texture_nearest_lod(imageTexture,IN.texCoords,0);
	vec2 dist		=depthToFadeDistance(depth.xy,2.0*(IN.texCoords-0.5),depthToLinFadeDistParams,tanHalfFov);
    vec4 result		=vec4(pow(dist.xy,0.44),depth.z,1.0);
	return result;
}

vec4 ShowDepthMS_PS(posTexVertexOutput IN) : SV_TARGET
{
	uint2 dims;
	uint numSamples;
	imageTextureMS.GetDimensions(dims.x,dims.y,numSamples);
	uint2 pos	=uint2(IN.texCoords.xy*vec2(dims.xy));
	vec4 depth		=imageTextureMS.Load(pos,0);
	vec2 dist		=depthToFadeDistance(depth.xx,2.0*(IN.texCoords-0.5),depthToLinFadeDistParams,tanHalfFov);
	return vec4(pow(dist.xy,0.44),depth.z,1.0);
}
struct vec3input
{
    float3 position	: POSITION;
};

v2f Vec3InputSignatureVS(vec3input IN)
{
	v2f OUT;
    OUT.hPosition=mul(worldViewProj,float4(IN.position.xyz,1.0));
	OUT.colour = float4(1.0,1.0,1.0,1.0);
    return OUT;
}

struct v2f_cubemap
{
    float4 hPosition	: SV_POSITION;
    float3 wDirection	: TEXCOORD0;
};


v2f_cubemap VS_DrawCubemap(vec3input IN) 
{
    v2f_cubemap OUT;
    OUT.hPosition	=mul(worldViewProj,float4(IN.position.xyz,1.0));
    OUT.wDirection	=normalize(IN.position.xyz);
    return OUT;
}

v2f_cubemap VS_DrawCubemapSphere(idOnly IN) 
{
    v2f_cubemap OUT;
	// we have (latitudes+1)*(longitudes+1)*2 id's
	uint vertex_id		=IN.vertex_id;
	uint latitude_strip	=vertex_id/(longitudes+1)/2;
	vertex_id			-=latitude_strip*(longitudes+1)*2;
	uint longitude		=(vertex_id)/2;
	vertex_id			-=longitude*2;
	float azimuth		=2.0*3.1415926536*float(longitude)/float(longitudes);
	float elevation		=.99*(float(latitude_strip+vertex_id)/float(latitudes+1)-0.5)*3.1415926536;
	float cos_el		=cos(elevation);
	vec3 pos			=radius*vec3(sin(azimuth)*cos_el,cos(azimuth)*cos_el,sin(elevation));
    OUT.hPosition		=mul(worldViewProj,float4(pos.xyz,1.0));
    OUT.wDirection		=normalize(pos.xyz);
    return OUT;
}


float4 PS_DrawCubemap(v2f_cubemap IN): SV_TARGET
{
	float3 view		=-(IN.wDirection.xyz);
	float4 result	=cubeTexture.Sample(cubeSamplerState,view);
	result.rgb		*=exposure;
	result.rgb		=pow(result.rgb,gamma);
	return float4(result.rgb,1.0);
}

fxgroup lines_3d_depth
{
	technique11 depth_forward
    {
		pass lines3d
		{
			SetRasterizerState( wireframeRasterizer );
			SetDepthStencilState( TestForwardDepth, 0 );
			SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_4_0,DebugVS()));
			SetPixelShader(CompileShader(ps_4_0,DebugPS()));
		}
    }
    technique11 depth_reverse
    {
		pass lines3d
		{
			SetRasterizerState( wireframeRasterizer );
			SetDepthStencilState( TestReverseDepth, 0 );
			SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_4_0,DebugVS()));
			SetPixelShader(CompileShader(ps_4_0,DebugPS()));
		}
    }
}
fxgroup lines_3d
{
	technique11 lines_3d
	{
		pass lines3d
		{
			SetRasterizerState( wireframeRasterizer );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_4_0,DebugVS()));
			SetPixelShader(CompileShader(ps_4_0,DebugPS()));
		}
	}
}
technique11 lines_2d
{
    pass p0
    {
		SetRasterizerState( wireframeRasterizer );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Debug2dVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}
fxgroup circle
{
	technique11 outline
	{
		pass p0
		{
			SetRasterizerState( wireframeRasterizer );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_4_0,CircleVS()));
			SetPixelShader(CompileShader(ps_4_0,DebugPS()));
		}
	}
	technique11 filled
	{
		pass p0
		{
			SetRasterizerState( RenderNoCull );
			SetDepthStencilState( DisableDepth, 0 );
			SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
			SetGeometryShader(NULL);
			SetVertexShader(CompileShader(vs_4_0,FilledCircleVS()));
			SetPixelShader(CompileShader(ps_4_0,DebugPS()));
		}
	}
}

technique11 textured
{
    pass noblend
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Quad()));
		SetPixelShader(CompileShader(ps_4_0,PS_ShowTexture()));
    }
    pass blend
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(AlphaBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Quad()));
		SetPixelShader(CompileShader(ps_4_0,PS_ShowTexture()));
    }
}

technique11 texturedMS
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Quad()));
		SetPixelShader(CompileShader(ps_5_0,PS_ShowTextureMS()));
    }
}

technique11 compacted_texture
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Quad()));
		SetPixelShader(CompileShader(ps_5_0,PS_CompactedTexture()));
    }
}

technique11 vec3_input_signature
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,Vec3InputSignatureVS()));
		SetPixelShader(CompileShader(ps_4_0,DebugPS()));
    }
}

technique11 draw_cubemap
{
    pass p0 
    {		
		SetRasterizerState( RenderFrontfaceCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_DrawCubemap()));
		SetPixelShader(CompileShader(ps_4_0,PS_DrawCubemap()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
    }
}
technique11 draw_cubemap_sphere
{
    pass p0 
    {		
		SetRasterizerState( RenderBackfaceCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_DrawCubemapSphere()));
		SetPixelShader(CompileShader(ps_4_0,PS_DrawCubemap()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4(0.0,0.0,0.0,0.0), 0xFFFFFFFF );
    }
}

technique11 show_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4( 0.0, 0.0, 0.0, 0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_Quad()));
		SetPixelShader(CompileShader(ps_4_0,ShowDepthPS()));
    }
}
technique11 show_depth_ms
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4( 0.0, 0.0, 0.0, 0.0), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Quad()));
		SetPixelShader(CompileShader(ps_5_0,ShowDepthMS_PS()));
    }
}