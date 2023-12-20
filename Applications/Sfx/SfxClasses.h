/* Copyright (c) 2011, Max Aizenshtein <max.sniffer@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the <organization> nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */


#pragma once

#include <map>
#include <string>
#include <sstream>
#include <vector>

int sfxparse();
int sfxlex();

namespace sfx
{
	typedef std::map<std::string, std::tuple<std::streampos, std::size_t>> BinaryMap;
	/// This must track platform::crossplatform::ShaderResourceType
	enum class ShaderResourceType : unsigned long long
	{
		UNKNOWN = 0
		, RW = 1
		, ARRAY = 2
		, MS = 4
		, TEXTURE = 8
		, TEXTURE_1D = TEXTURE | 16
		, TEXTURE_2D = TEXTURE | 32
		, TEXTURE_3D = TEXTURE | 64
		, TEXTURE_CUBE = TEXTURE | 128
		, SAMPLER = 256
		, BUFFER = 512
		, CBUFFER = 1024
		, TBUFFER = 2048
		, BYTE_ADDRESS_BUFFER = 4096
		, STRUCTURED_BUFFER = 8192
		, APPEND_STRUCTURED_BUFFER = 16384 | STRUCTURED_BUFFER
		, CONSUME_STRUCTURED_BUFFER = 32768 | STRUCTURED_BUFFER
		, TEXTURE_2DMS = TEXTURE_2D | MS
		, RW_TEXTURE_1D = TEXTURE_1D | RW
		, RW_TEXTURE_2D = TEXTURE_2D | RW
		, RW_TEXTURE_3D = TEXTURE_3D | RW
		, RW_BUFFER = BUFFER | RW
		, RW_BYTE_ADDRESS_BUFFER = RW | BYTE_ADDRESS_BUFFER
		, RW_STRUCTURED_BUFFER = RW | STRUCTURED_BUFFER
		, TEXTURE_1D_ARRAY = TEXTURE_1D | ARRAY
		, TEXTURE_2D_ARRAY = TEXTURE_2D | ARRAY
		, TEXTURE_3D_ARRAY = TEXTURE_3D | ARRAY
		, TEXTURE_2DMS_ARRAY = TEXTURE_2D | MS | ARRAY
		, TEXTURE_CUBE_ARRAY = TEXTURE_CUBE | ARRAY
		, RW_TEXTURE_1D_ARRAY = RW | TEXTURE_1D | ARRAY
		, RW_TEXTURE_2D_ARRAY = RW | TEXTURE_2D | ARRAY
		, RW_TEXTURE_3D_ARRAY = RW | TEXTURE_3D | ARRAY
		, RAYTRACE_ACCELERATION_STRUCT =65536
		, NAMED_CONSTANT_BUFFER =131072
		, COUNT
	};
	inline ShaderResourceType operator|(ShaderResourceType a, ShaderResourceType b)
	{
		return static_cast<ShaderResourceType>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
	}
	inline ShaderResourceType operator&(ShaderResourceType a, ShaderResourceType b)
	{
		return static_cast<ShaderResourceType>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
	}
	/// Values that represent ShaderType.
	enum ShaderType
	{
		UNKNOWN_SHADER_TYPE=0,
		VERTEX_SHADER,
		TESSELATION_CONTROL_SHADER,		//= Hull shader
		TESSELATION_EVALUATION_SHADER,	//= Domain Shader
		GEOMETRY_SHADER,
		FRAGMENT_SHADER,
		COMPUTE_SHADER,
		RAY_GENERATION_SHADER,
		MISS_SHADER,
		CALLABLE_SHADER,
		CLOSEST_HIT_SHADER,
		ANY_HIT_SHADER,
		INTERSECTION_SHADER,
		EXPORT_SHADER,
		NUM_SHADER_TYPES
	};
	enum RenderStateType
	{
		RASTERIZER_STATE
		,DEPTHSTENCIL_STATE
		,BLEND_STATE
		,SAMPLER_STATE	
		,RENDERTARGETFORMAT_STATE
		,NUM_RENDERSTATE_TYPES
	};
	enum Topology
	{
		UNDEFINED			
		,POINTLIST			
		,LINELIST			
		,LINESTRIP			
		,TRIANGLELIST		
		,TRIANGLESTRIP		
		,LINELIST_ADJACENCY
		,LINESTRIP_ADJACENCY	
		,TRIANGLELIST_ADJACENCY
		,TRIANGLESTRIP_ADJACENCY
		,NUM_TOPOLOGIES
	};
	enum BlendOption
	{
		BLEND_ZERO
		,BLEND_ONE
		,BLEND_SRC_COLOR
		,BLEND_INV_SRC_COLOR
		,BLEND_SRC_ALPHA
		,BLEND_INV_SRC_ALPHA
		,BLEND_DEST_ALPHA
		,BLEND_INV_DEST_ALPHA
		,BLEND_DEST_COLOR
		,BLEND_INV_DEST_COLOR
		,BLEND_SRC_ALPHA_SAT
		,BLEND_BLEND_FACTOR
		,BLEND_INV_BLEND_FACTOR
		,BLEND_SRC1_COLOR
		,BLEND_INV_SRC1_COLOR
		,BLEND_SRC1_ALPHA
		,BLEND_INV_SRC1_ALPHA
	};
	enum BlendOperation
	{
		BLEND_OP_NONE		//opaque
		,BLEND_OP_ADD
		,BLEND_OP_SUBTRACT
		,BLEND_OP_MAX
		,BLEND_OP_MIN
	} ;
	enum DepthComparison
	{
		DEPTH_NEVER,
		DEPTH_ALWAYS,
		DEPTH_LESS,
		DEPTH_EQUAL,
		DEPTH_LESS_EQUAL,
		DEPTH_GREATER,
		DEPTH_NOT_EQUAL,
		DEPTH_GREATER_EQUAL
	};
	enum PixelOutputFormat
	{
		FMT_UNKNOWN
		,FMT_32_GR
		,FMT_32_AR 
		,FMT_FP16_ABGR 
		,FMT_UNORM16_ABGR 
		,FMT_SNORM16_ABGR 
		,FMT_UINT16_ABGR 
		,FMT_SINT16_ABGR 
		,FMT_32_ABGR 
		,OUTPUT_FORMAT_COUNT
	};
	enum ShaderCommand
	{
		Unknown=0
		,SetVertexShader		//VS Vertex Shader			|	Vertex Shader
		,SetHullShader			//TC Tessellation Control	|	Hull Shader
		,SetDomainShader		//TE Tessellation Evaluation	|	Domain Shader
		,SetGeometryShader		//GS Geometry Shader			|	Geometry Shader
		,SetPixelShader			//FS Fragment Shader			|	Pixel Shader
		,SetComputeShader		//CS Compute Shader			|	Compute Shader
		,SetRayGenerationShader
		,SetMissShader
		,SetCallableShader
		,SetClosestHitShader
		,SetAnyHitShader
		,SetIntersectionShader
		,SetExportShader		// this is a PS4 thing. We will write Vertex shaders as export shaders when necessary.
		,NUM_OF_SHADER_TYPES
		,SetRasterizerState
		,SetDepthStencilState
		,SetBlendState
		,SetRenderTargetFormatState
		,SetTopology			// New for SFX
		,NUM_OF_SHADER_COMMANDS
	};
	enum class DeclarationType
	{
		TEXTURE,SAMPLER,BLENDSTATE,RASTERIZERSTATE,DEPTHSTATE,BUFFER,STRUCT,CONSTANT_BUFFER, NAMED_CONSTANT_BUFFER,RENDERTARGETFORMAT_STATE,VARIABLE
	};
	struct Declaration
	{
		Declaration(DeclarationType t):
			global_line_number(0)
			,file_number(0)
			,line_number(0)
			,ref_count(0)
			,declarationType(t)
		{
		}
		virtual ~Declaration()
		{
			// this makes it polymorphic.
		}
		int global_line_number;
		int file_number;
		int line_number;
		int ref_count;
		std::string name;
		std::string original;
		std::string structureType; // e.g. Buffer<structType> if used.
		DeclarationType declarationType;
		// Not used for all:
		int slot = 0;
		// Mainly Vulkan descriptorSets:
		int group_num=0;
		int space = 0;
		std::string type;
	};
	struct Variable : public Declaration
	{
		Variable():Declaration(DeclarationType::VARIABLE)
		{
		}
		std::string init_declarator_list;
	};
	struct BlendState: public Declaration
	{
		BlendState();
		BlendOption SrcBlend;			
		BlendOption DestBlend;			
		BlendOperation BlendOp;				
		BlendOption SrcBlendAlpha;		
		BlendOption DestBlendAlpha;		
		BlendOperation BlendOpAlpha;		
		bool AlphaToCoverageEnable;
		std::map<int,bool> BlendEnable;
		std::map<int,unsigned char> RenderTargetWriteMask;
	};
	enum FillMode
	{ 
		FILL_WIREFRAME
		,FILL_SOLID
		,FILL_POINT
	};
	enum CullMode
	{ 
		CULL_NONE
		,CULL_FRONT 
		,CULL_BACK  
	};
	enum FilterMode
	{ 
		MIN_MAG_MIP_LINEAR  
		,MIN_MAG_MIP_POINT
		,ANISOTROPIC
	};
	enum AddressMode
	{ 
		CLAMP,WRAP,MIRROR
	};
	struct DepthStencilState: public Declaration
	{
		DepthStencilState();
		bool DepthTestEnable;
		int DepthWriteMask;
		DepthComparison DepthFunc;
	};
	struct RasterizerState: public Declaration
	{
		RasterizerState();
		FillMode	fillMode;
		CullMode	cullMode;
		bool		FrontCounterClockwise;
		int			DepthBias;
		float		DepthBiasClamp;
		float		SlopeScaledDepthBias;
		bool		DepthClipEnable;
		bool		ScissorEnable;
		bool		MultisampleEnable;
		bool		AntialiasedLineEnable;
	};
	struct RenderTargetFormatState: public Declaration
	{
		RenderTargetFormatState();
		PixelOutputFormat formats[8];
	};
	struct SamplerState : public Declaration
	{
		SamplerState()
		: Declaration (DeclarationType::SAMPLER)
			,register_number(-1)
		,Filter(MIN_MAG_MIP_LINEAR)
		,AddressU(WRAP)
		,AddressV(WRAP)
		,AddressW(WRAP)
		{
		}
		int register_number;
		FilterMode Filter;
		AddressMode AddressU;
		AddressMode AddressV;
		AddressMode AddressW;
		DepthComparison depthComparison;
	};
	//! Types of test for variant variables.
	enum VariantTest
	{
		NoTest = 0,
		NotEqual = 1,
		Equal = 2,
		Greater = 4,
		Less = 8,
		GreaterEqual = Greater | Equal,
		LessEqual = Less | Equal
	};
	//! What to compare variant variables to.
	union VariantComparator
	{
		float fval;
		int ival;
	};
	struct StructMember
	{
		std::string type;
		std::string name;
		std::string semantic;
		std::string variantCondition;// allows shader variants to modify struct makeup.
		std::string variantVariable;
		VariantTest variantTest;
		VariantComparator variantComparator;
	};
	struct Struct: public Declaration
	{
		Struct(DeclarationType t= DeclarationType::STRUCT)
			: Declaration (t)
		{
		}
		std::vector<StructMember> m_structMembers;
		bool hasVariants=false;
	};
	struct ConstantBuffer : public Struct
	{
		ConstantBuffer(DeclarationType t= DeclarationType::CONSTANT_BUFFER):Struct(t)
		{
		}
	};

	struct DeclaredResource: public Declaration
	{
		DeclaredResource(DeclarationType t):Declaration(t)
		{
		}
	};

	struct DeclaredTexture: public DeclaredResource
	{
		DeclaredTexture():DeclaredResource(DeclarationType::TEXTURE)
		{
		}
		bool variant;			// if true, we must define different versions for different texture output formats.
		std::string layout;
		std::string texel_format;
		ShaderResourceType shaderResourceType;
	};

	struct NamedConstantBuffer: public ConstantBuffer
	{
		NamedConstantBuffer():ConstantBuffer(DeclarationType::NAMED_CONSTANT_BUFFER)
		{
		}
		std::string instance_name;
	};

	struct PassRasterizerState
	{
		std::string objectName;
	};

	struct PassRenderTargetFormatState
	{
		std::string objectName;
	};

	struct PassDepthStencilState
	{
		std::string objectName;
		int stencilRef;
	};

	struct PassBlendState
	{
		PassBlendState():sampleMask(0)
		{
			blendFactor[0]=blendFactor[1]=blendFactor[2]=blendFactor[3]=0.0f;
		}
		std::string objectName;
		float blendFactor[4];
		unsigned sampleMask;
	};

	struct TopologyState
	{
		TopologyState():apply(false),topology(sfx::Topology::NUM_TOPOLOGIES)
		{}

		bool apply;
		Topology topology;
	};
	struct RaytraceHitGroup
	{
		std::string closestHit;
		std::string anyHit;
		std::string intersection;
	};
	struct PassState
	{
		PassState()
		{
		}
		PassRasterizerState rasterizerState;
		PassDepthStencilState depthStencilState;
		PassBlendState blendState;
		PassRenderTargetFormatState renderTargetFormatState;
		TopologyState topologyState;
		std::map<ShaderType,std::string> shaders;
		std::map<std::string,RaytraceHitGroup> raytraceHitGroups;
		std::vector<std::string> missShaders;
		std::vector<std::string> callableShaders;
		int maxPayloadSize = 0;
		int maxAttributeSize = 0;
		int maxTraceRecursionDepth = 0;
	};
}
#include "SfxProgram.h"
