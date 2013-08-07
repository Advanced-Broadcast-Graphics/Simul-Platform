#ifndef CPP_HLSL
#define CPP_HLSL
#include "../../CrossPlatform/CppSl.hs"

#ifndef __cplusplus
#define texture_clamp_mirror(tex,texc) tex.Sample(samplerStateClampMirror,texc)
#define texture_clamp(tex,texc) tex.Sample(clampSamplerState,texc)
#define texture_wrap_clamp(tex,texc) tex.Sample(wrapClampSamplerState,texc)
#define texture_wrap_mirror(tex,texc) tex.Sample(wrapMirrorSamplerState,texc)
#define sampleLod(tex,sampler,texc,lod) tex.SampleLevel(sampler,texc,lod)
#define texture(tex,texc) tex.Sample(samplerState,texc)
#define texture2D(tex,texc) tex.Sample(samplerState,texc)
#define texture_wrap(tex,texc) tex.Sample(wrapSamplerState,texc)
#define texture_wwc(tex,texc) tex.Sample(wwcSamplerState,texc)
#define texture3Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture2Dpt(tex,texc) tex.Sample(samplerStateNearest,texc)
#define texture(tex,texc) tex.Sample(samplerState,texc)
#endif

#define uniform
#define uniform_buffer ALIGN_16 cbuffer
#define sampler1D texture1D
#define sampler2D texture2D
#define sampler3D texture3D
#define STATIC static

#ifndef __cplusplus
	#define SIMUL_TEXTURE_REGISTER(buff_num) : register(t##buff_num)
	#define SIMUL_SAMPLER_REGISTER(buff_num) : register(s##buff_num)
	#define SIMUL_BUFFER_REGISTER(buff_num) : register(b##buff_num)
	#define R0 : register(b0)
	#define R1 : register(b1)
	#define R2 : register(b2)
	#define R3 : register(b3)
	#define R4 : register(b4)
	#define R5 : register(b5)
	#define R6 : register(b6)
	#define R7 : register(b7)
	#define R8 : register(b8)
	#define R9 : register(b9)
	#define R10 : register(b10)
	#define R13 : register(b13)
	#define vec1 float1
	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define mat2 float2x2
	#define mat3 float3x3
	#define mat4 float4x4
	#define mix lerp
	#define fract frac

	#define Y(texel) texel.z
	
	SamplerState samplerStateClampMirror 
	{
		Filter = MIN_MAG_MIP_LINEAR;
		AddressU = Clamp;
		AddressV = Mirror;
	};

	struct idOnly
	{
		uint vertex_id			: SV_VertexID;
	};
#endif

#endif