#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/FileLoader.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/StringToWString.h"
#include <iostream>
#include <algorithm>
#include <regex>		// for file loading

#if PLATFORM_STD_FILESYSTEM > 0
#include <filesystem>
#endif

using namespace platform;
using namespace crossplatform;
using namespace std;
using namespace platform;
using namespace core;

////////////////////
//Helper Functions//
////////////////////

static bool is_equal(std::string str, const char *tst)
{
	return (_stricmp(str.c_str(), tst) == 0);
}

static SamplerStateDesc::Wrapping stringToWrapping(string s)
{
	if (is_equal(s, "WRAP"))
		return SamplerStateDesc::WRAP;
	if (is_equal(s, "CLAMP"))
		return SamplerStateDesc::CLAMP;
	if (is_equal(s, "MIRROR"))
		return SamplerStateDesc::MIRROR;
	SIMUL_BREAK_ONCE((string("Invalid string") + s).c_str());
	return SamplerStateDesc::WRAP;
}

static SamplerStateDesc::Filtering stringToFilter(string s)
{
	if (is_equal(s, "POINT"))
		return SamplerStateDesc::POINT;
	if (is_equal(s, "LINEAR"))
		return SamplerStateDesc::LINEAR;
	if (is_equal(s, "ANISOTROPIC"))
		return SamplerStateDesc::ANISOTROPIC;
	SIMUL_BREAK((string("Invalid string: ") + s).c_str());
	return SamplerStateDesc::POINT;
}

static int toInt(string s)
{
	const char *t = s.c_str();
	return atoi(t);
}

static CullFaceMode toCullFadeMode(string s)
{
	if (is_equal(s, "CULL_BACK"))
		return CULL_FACE_BACK;
	else if (is_equal(s, "CULL_FRONT"))
		return CULL_FACE_FRONT;
	else if (is_equal(s, "CULL_FRONTANDBACK"))
		return CULL_FACE_FRONTANDBACK;
	else if (is_equal(s, "CULL_NONE"))
		return CULL_FACE_NONE;
	SIMUL_BREAK((string("Invalid string") + s).c_str());
	return CULL_FACE_NONE;
}

static Topology toTopology(string s)
{
	if (is_equal(s, "PointList"))
		return Topology::POINTLIST;
	else if (is_equal(s, "LineList"))
		return Topology::LINELIST;
	else if (is_equal(s, "LineStrip"))
		return Topology::LINESTRIP;
	else if (is_equal(s, "TriangleList"))
		return Topology::TRIANGLELIST;
	else if (is_equal(s, "TriangleStrip"))
		return Topology::TRIANGLESTRIP;
	else if (is_equal(s, "LineListAdjacency"))
		return Topology::LINELIST_ADJ;
	else if (is_equal(s, "LineStripAdjacency"))
		return Topology::LINESTRIP_ADJ;
	else if (is_equal(s, "TriangleListAdjacency"))
		return Topology::TRIANGLELIST_ADJ;
	else if (is_equal(s, "TriangleStripAdjacency"))
		return Topology::TRIANGLESTRIP_ADJ;
	SIMUL_BREAK((string("Invalid string") + s).c_str());
	return Topology::UNDEFINED;
}

static PolygonMode toPolygonMode(string s)
{
	if (is_equal(s, "FILL_SOLID"))
		return PolygonMode::POLYGON_MODE_FILL;
	else if (is_equal(s, "FILL_WIREFRAME"))
		return PolygonMode::POLYGON_MODE_LINE;
	else if (is_equal(s, "FILL_POINT"))
		return PolygonMode::POLYGON_MODE_POINT;
	return PolygonMode::POLYGON_MODE_FILL;
}

static bool toBool(string s)
{
	string ss = s.substr(0, 4);
	const char *t = ss.c_str();
	if (_stricmp(t, "true") == 0)
		return true;
	ss = s.substr(0, 5);
	t = ss.c_str();
	if (_stricmp(t, "false") == 0)
		return false;
	SIMUL_CERR << "Unknown bool " << s << std::endl;
	return false;
}

//////////
//Shader//
//////////

void Shader::setUsesTextureSlot(int s)
{
	if (textureSlotsForSB[s])
	{
		SIMUL_BREAK_ONCE("Can't use slot for Texture and Structured Buffer in the same shader.");
	}
	textureSlots |= Slots(s);
}
void Shader::setUsesTextureSlotForSB(int s)
{
	if (textureSlots[s])
	{
		SIMUL_BREAK_ONCE("Can't use slot for Texture and Structured Buffer in the same shader.");
	}
	textureSlotsForSB |= Slots(s);
}
void Shader::setUsesRwTextureSlot(int s)
{
	rwTextureSlots |= Slots(s);
}
void Shader::setUsesRwTextureSlotForSB(int s)
{
	if (rwTextureSlots[s])
	{
		SIMUL_BREAK_ONCE("Can't use slot for RW Texture and Structured Buffer in the same shader.");
	}
	rwTextureSlotsForSB |= Slots(s);
}
void Shader::setUsesSamplerSlot(int s)
{
	samplerSlots |= Slots(s);
}
void Shader::setUsesConstantBufferSlot(int s)
{
	constantBufferSlots |= Slots(s);
}

bool Shader::usesTextureSlot(int s) const
{
	return textureSlots[s];
}
bool Shader::usesTextureSlotForSB(int s) const
{
	return textureSlotsForSB[s];
}
bool Shader::usesRwTextureSlot(int s) const
{
	return rwTextureSlots[s];
}
bool Shader::usesRwTextureSlotForSB(int s) const
{
	return rwTextureSlotsForSB[s];
}
bool Shader::usesSamplerSlot(int s) const
{
	return true; //samplerSlots[s];
}
bool Shader::usesConstantBufferSlot(int s) const
{
	return constantBufferSlots[s];
}

//////////////
//EffectPass//
//////////////

EffectPass::EffectPass(RenderPlatform *r,Effect *parent)
	:blendState(nullptr)
	,depthStencilState(nullptr)
	,rasterizerState(nullptr)
	,renderTargetFormatState(nullptr)
	,should_fence_outputs(true)
	,platform_pass(nullptr)
	,renderPlatform(r)
	,effect(parent)
{
	for(int i=0;i<SHADERTYPE_COUNT;i++)
		shaders[i]=nullptr;
	for(int i=0;i<OUTPUT_FORMAT_COUNT;i++)
		pixelShaders[i]=nullptr;
}
EffectPass::~EffectPass()
{
	for(int i=0;i<SHADERTYPE_COUNT;i++)
	{
		shaders[i]=nullptr;
	}
	for(int i=0;i<OUTPUT_FORMAT_COUNT;i++)
	{
		if(pixelShaders[i])
			pixelShaders[i]=nullptr;
	}
}

void EffectPass::MakeResourceSlotMap()
{
	auto CollectSlotNumbers = [&](const Slots &s) -> std::vector<int>
	{
		const size_t &count = s.count();
		std::vector<int> result;
		result.reserve(count);
		for(size_t i = 0; i < s.size(); i++)
		{
			if (s[i])
			{
				result.push_back((int)i);
			}
			if (count == result.size())
			{
				break;
			}
		}
		return result;
	};

	collectedResourceSlots = CollectSlotNumbers(resourceSlots);
	collectedRwResourceSlots = CollectSlotNumbers(rwResourceSlots);
	collectedSbResourceSlots = CollectSlotNumbers(sbResourceSlots);
	collectedRwSbResourceSlots = CollectSlotNumbers(rwSbResourceSlots);
	collectedSamplerResourceSlots = CollectSlotNumbers(samplerResourceSlots);
	collectedConstantBufferResourceSlots = CollectSlotNumbers(constantBufferResourceSlots);
}

bool EffectPass::usesTextureSlot(int s) const
{
 	if(s>=1000)
		return usesRwTextureSlot(s-1000);
	return resourceSlots[s];
}
bool EffectPass::usesRwTextureSlot(int s) const
{
	return rwResourceSlots[s];
}
bool EffectPass::usesTextureSlotForSB(int s) const
{
	if(s>=1000)
		return usesRwTextureSlotForSB(s-1000);
	return sbResourceSlots[s];
}
bool EffectPass::usesRwTextureSlotForSB(int s) const
{
	return rwSbResourceSlots[s];
}
bool EffectPass::usesSamplerSlot(int s) const
{
	return samplerResourceSlots[s];
}
bool EffectPass::usesConstantBufferSlot(int s) const
{
	return constantBufferResourceSlots[s];
}

bool EffectPass::usesTextures() const
{
	return resourceSlots.any() || rwResourceSlots.any();
}
bool EffectPass::usesRwTextures() const
{
	return rwResourceSlots.any();
}
bool EffectPass::usesSBs() const
{
	return sbResourceSlots.any();
}
bool EffectPass::usesRwSBs() const
{
	return rwSbResourceSlots.any();
}
bool EffectPass::usesSamplers() const
{
	return samplerResourceSlots.any();
}
bool EffectPass::usesConstantBuffers() const
{
	return constantBufferResourceSlots.any();
}

void EffectPass::SetUsesTextureSlots(const Slots& s)
{
	resourceSlots |= s;
}
void EffectPass::SetUsesRwTextureSlots(const Slots& s)
{
	rwResourceSlots |= s;
}
void EffectPass::SetUsesTextureSlotsForSB(const Slots& s)
{
	sbResourceSlots |= s;
}
void EffectPass::SetUsesRwTextureSlotsForSB(const Slots& s)
{
	rwSbResourceSlots |= s;
}
void EffectPass::SetUsesSamplerSlots(const Slots& s)
{
	samplerResourceSlots |= s;
}
void EffectPass::SetUsesConstantBufferSlots(const Slots& s)
{
	constantBufferResourceSlots |= s;
}

void EffectPass::CheckSlots(crossplatform::Slots requiredSlots, crossplatform::Slots usedSlots, int numSlots, const char *type)
{
	if (requiredSlots != usedSlots)
	{
		crossplatform::Slots missingSlots = requiredSlots & (~usedSlots);
		for (int i = 0; i < numSlots; i++)
		{
			if (missingSlots[i])
			{
				SIMUL_INTERNAL_CERR << "Resource binding error at: " << name << ". " << type << " for slot:" << i << " was not set!\n";
			}
		}
	}
}

/////////////////////
//EffectVariantPass//
/////////////////////

EffectPass* EffectVariantPass::GetPass(const char *shader1,const char *shader2)
{
	for(auto i:passes)
	{
		vector<string> parts=platform::core::split(i.first,'.');
		if(parts[1]==shader1)
		{
			if(!shader2||parts[2]==shader2)
				return i.second;
		}
	}
	return nullptr;
}

///////////////////
//EffectTechnique//
///////////////////

EffectTechnique::EffectTechnique(RenderPlatform *r,Effect *e)
	:platform_technique(nullptr)
	,should_fence_outputs(true)
	, renderPlatform(r)
	, effect(e)
{
}
EffectTechnique::~EffectTechnique()
{
	for(auto p:passes_by_name)
	{
		delete p.second;
	}
	passes_by_name.clear();
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_name.size();
}

EffectVariantPass *EffectTechnique::AddVariantPass(const char *name)
{
	auto vp=std::make_shared<EffectVariantPass>();
	vp->name=name;
	variantPasses[name]=vp;
	return vp.get();
}
EffectVariantPass *EffectTechnique::GetVariantPass(const char *name)
{
	auto i=variantPasses.find(name);
	if(i==variantPasses.end())
		return nullptr;
	return i->second.get();
}

EffectPass *EffectTechnique::GetPass(int i) const
{
	return passes_by_index.at(i);
}
EffectPass* EffectTechnique::GetPass(const char* name) const
{
	auto p = passes_by_name.find(name);
	if(p==passes_by_name.end())
		return nullptr;
	return p->second;
}
bool EffectTechnique::HasPass(int i) const
{
	return (passes_by_index.find(i) != passes_by_index.end());
}
bool EffectTechnique::HasPass(const char* name) const
{
	return (passes_by_name.find(name) != passes_by_name.end());
}

////////////////////////
//EffectTechniqueGroup//
////////////////////////

EffectTechniqueGroup::~EffectTechniqueGroup()
{
	for (TechniqueMap::iterator i = techniques.begin(); i != techniques.end(); i++)
	{
		delete i->second;
	}
	techniques.clear();
	charMap.clear();
}
EffectTechnique *EffectTechniqueGroup::GetTechniqueByName(const char *name)
{
	TechniqueCharMap::iterator i=charMap.find(name);
	if(i!=charMap.end())
		return i->second;
	TechniqueMap::iterator j=techniques.find(name);
	if(j==techniques.end())
		return nullptr;
	charMap[name]=j->second;
	return j->second;
}
EffectTechnique *EffectTechniqueGroup::GetTechniqueByIndex(int index)
{
	return techniques_by_index[index];
}

//////////
//Effect//
//////////

Effect::Effect()
	:renderPlatform(nullptr)
	,platform_effect(nullptr)
{
}
Effect::~Effect()
{
	InvalidateDeviceObjects();
	for (auto& i : depthStencilStates)
	{
		delete i.second;
	}
	for (auto& i : blendStates)
	{
		delete i.second;
	}
	for (auto& i : rasterizerStates)
	{
		delete i.second;
	}
	for (auto& i : rtFormatStates)
	{
		delete i.second;
	}
}

void Effect::InvalidateDeviceObjects()
{
	shaderResources.clear();
	for (auto i = groups.begin(); i != groups.end(); i++)
	{
		delete i->second;
	}
	groups.clear();
	groupCharMap.clear();
	techniqueCharMap.clear();
	techniques.clear();
	// We don't own the sampler states in effects.
	samplerStates.clear();
	for (auto& i : depthStencilStates)
	{
		i.second->InvalidateDeviceObjects();
	}
	depthStencilStates.clear();
	for (auto& i : blendStates)
	{
		i.second->InvalidateDeviceObjects();
	}
	blendStates.clear();
	for (auto& i : rasterizerStates)
	{
		i.second->InvalidateDeviceObjects();
	}
	rasterizerStates.clear();
	for (auto& i : rtFormatStates)
	{
		i.second->InvalidateDeviceObjects();
	}
	rtFormatStates.clear();

	for (auto i : textureDetailsMap)
	{
		delete i.second;
	}
	textureDetailsMap.clear();
}

bool Effect::Load(RenderPlatform *r, const char *filename_utf8)
{
	renderPlatform=r;
	filename=filename_utf8;
	// Clear the effect
	InvalidateDeviceObjects();
	for(auto i:textureDetailsMap)
	{
		delete i.second;
	}
	textureDetailsMap.clear();
	textureCharMap.clear();
	// We will load the .sfxo file, which contains the list of shader binary files, and also the arrangement of textures, buffers etc. in numeric slots.
	std::vector<std::string> binaryPaths	=renderPlatform->GetShaderBinaryPathsUtf8();
	std::string filenameUtf8				=filename_utf8;
	std::string binFilenameUtf8				=filenameUtf8;

	if (binFilenameUtf8.find(".sfxo") == std::string::npos)
		binFilenameUtf8 += ".sfxo";
	int index = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(binFilenameUtf8.c_str(), binaryPaths);
	std::string filepathUtf8;
	if (index < 0 || index >= binaryPaths.size())
		filepathUtf8 = "";
	else if (index < binaryPaths.size())
		filepathUtf8 = binaryPaths[index];

	binFilenameUtf8 = filepathUtf8 +"/"s+ binFilenameUtf8;
	platform::core::find_and_replace(binFilenameUtf8,"\\","/");
	if(!platform::core::FileLoader::GetFileLoader()->FileExists(binFilenameUtf8.c_str()))
	{
		std::transform(binFilenameUtf8.begin(), binFilenameUtf8.end(), binFilenameUtf8.begin(), ::tolower);
		if(!platform::core::FileLoader::GetFileLoader()->FileExists(binFilenameUtf8.c_str()))
		{
			string err= platform::core::QuickFormat("Shader effect file not found: %s",binFilenameUtf8.c_str());
			SIMUL_BREAK_ONCE(err.c_str());
			static bool already = false;
			if (!already)
			{
			#if PLATFORM_STD_FILESYSTEM > 0
				std::cerr << "Current path is: " << std::filesystem::current_path().string().c_str()<<std::endl;
			#endif
				std::cerr << "Binary paths searched: "<< std::endl;
				for (auto p : binaryPaths)
				{
					std::cerr << "\t"<<p.c_str() << std::endl;
				}
				already = true;
			}
			if(!platform::core::FileLoader::GetFileLoader()->FileExists(binFilenameUtf8.c_str()))
			{
				binFilenameUtf8 =filename_utf8;
				// The sfxo does not exist, so we can't load this effect.
				return false;
			}
		}
	}
	void *ptr;
	unsigned int num_bytes;

	std::string sfxbFilenameUtf8 = binFilenameUtf8;
	platform::core::find_and_replace(sfxbFilenameUtf8, ".sfxo", ".sfxb");

	platform::core::FileLoader::GetFileLoader()->AcquireFileContents(ptr,num_bytes, binFilenameUtf8.c_str(),true);
	filenameInUseUtf8=binFilenameUtf8;
	void *bin_ptr=nullptr;
	unsigned int bin_num_bytes=0;

	const char *txt=(const char *)ptr;
	std::string str;
	str.reserve(num_bytes);
	str.resize((size_t)num_bytes, 0);
	memcpy(const_cast<char*>(str.data()),txt,num_bytes);
	// Load all the .sb's
	int pos					=0;
	int next				=(int)str.find('\n',pos+1);
	//int line_number			=0;
	enum Level
	{
		OUTSIDE=0,GROUP=1,TECHNIQUE=2,VARIANT_PASS=3,PASS=4,LAYOUT=5,HITGROUP=6,MISS_SHADERS=6,CALLABLE_SHADERS=6,RAYTRACING_CONFIG=6,TOO_FAR=7
	};
	Level level				=OUTSIDE;
	bool variant_mode		=false;
	EffectTechnique *tech	=nullptr;
	EffectVariantPass *variantPass=nullptr;
	EffectPass *p			=nullptr;
	RaytraceHitGroup *hg	=nullptr;
	LayoutDesc layoutDesc[32];
	int layoutCount=0;
	int layoutOffset=0;
	int layoutSlot=0;
	int passNum=0;
	string group_name,tech_name,pass_name;
	int shaderCount=0;
	bool platformChecked = false;
	while(next>=0)
	{
		#ifdef UNIX
		string line		=str.substr(pos,next-pos);
		#else
		string line		=str.substr(pos,next-pos-1);
		#endif
		platform::core::ClipWhitespace(line);
		if (!platformChecked)
		{
			if (line.substr(0, 3) != std::string("SFX"))
			{
				SIMUL_BREAK_ONCE("No SFX init string in effect file ");
				return false;
			}
			else
			{
				string platformString = line.substr(4, line.length() - 4);
				if (platformString != string(renderPlatform->GetName()))
				{
					SIMUL_CERR << "Platform " << platformString.c_str() << " from file " << filenameUtf8.c_str() << " does not match platform " << renderPlatform->GetName() << "\n";
					SIMUL_BREAK_ONCE("Invalid platform");
					return false;
				}
			}
			platformChecked = true;
		}
		vector<string> words= platform::core::split(line,' ');
		pos				=next;
		int sp=(int)line.find(" ");
		int open_brace= (int)line.find("{");
		if(open_brace>=0)
		{
			if(level!=HITGROUP || level!=MISS_SHADERS || level!=CALLABLE_SHADERS || level!=RAYTRACING_CONFIG)
			{
				level=(Level)(level+1);
				if(level==VARIANT_PASS&&!variant_mode)
					level=PASS;
			}
		}
		string word;
		if(sp >= 0)
			word=line.substr(0, sp);
		if(level==OUTSIDE)
		{
			tech=nullptr;
			p=nullptr;
			if (is_equal(word, "group") )
			{
				group_name = line.substr(sp + 1, line.length() - sp - 1);
			}
			else if(is_equal(word,"accelerationstructure"))
			{
				const string &name	=words[1];
				const string &register_num	=words[4];
				int slot=atoi(register_num.c_str());
				ShaderResource *res=new ShaderResource;
				res->slot				=slot;
				res->shaderResourceType=ShaderResourceType::ACCELERATION_STRUCTURE;
				textureDetailsMap[name]=res;
				textureResources[slot]=res;
			}
			else if(is_equal(word,"texture"))
			{
				const string &texture_name	=words[1];
				const string &texture_dim	=words[2];
				const string &read_write	=words[3];
				const string &register_num	=words[4];
				string is_array				=words.size()>5?words[5]:"single";
				int slot=atoi(register_num.c_str());
				int dim=is_equal(texture_dim,"3d")||is_equal(texture_dim,"3dms")?3:2;
				bool is_cubemap=is_equal(texture_dim,"cubemap")||is_equal(texture_dim,"cubemapms");
				bool is_msaa=texture_dim.find("ms")!=string::npos;
				bool rw=is_equal(read_write,"read_write");
				bool ar=is_equal(is_array,"array");
				ShaderResource *res=new ShaderResource;
				res->slot				=slot;
				res->dimensions			=dim;
				ShaderResourceType rt=ShaderResourceType::COUNT;
				if(!rw)
				{
					if(is_cubemap)
					{
							rt	=ShaderResourceType::TEXTURE_CUBE;
					}
					else
					{
						switch(dim)
						{
						case 1:
							rt	=ShaderResourceType::TEXTURE_1D;
							break;
						case 2:
							rt	=ShaderResourceType::TEXTURE_2D;
							break;
						case 3:
							rt	=ShaderResourceType::TEXTURE_3D;
							break;
						default:
							break;
						}
					}
				}
				else
				{
					switch(dim)
					{
					case 1:
						rt	=ShaderResourceType::RW_TEXTURE_1D;
						break;
					case 2:
						rt	=ShaderResourceType::RW_TEXTURE_2D;
						break;
					case 3:
						rt	=ShaderResourceType::RW_TEXTURE_3D;
						break;
					default:
						break;
					}
				}
				if(ar)
					rt=rt|ShaderResourceType::ARRAY;
				if (is_msaa)
					rt=rt|ShaderResourceType::MS;
				res->shaderResourceType=rt;
				textureDetailsMap[texture_name]=res;
				if(!rw)
					textureResources[slot]=res;
			}
			else if(is_equal(word, "BlendState"))
			{
				string name		=words[1];
				string props	=words[2];
				size_t pos_b		=0;
				RenderStateDesc desc;
				desc.name=name;
				desc.type=BLEND;
				desc.blend.AlphaToCoverageEnable=toBool(platform::core::toNext(props,',', pos_b));
				pos_b++;
				string enablestr=platform::core::toNext(props,')', pos_b);
				vector<string> en= platform::core::split(enablestr,',');

				desc.blend.numRTs= (int)en.size();
				pos_b++;
				BlendOperation BlendOp		=(BlendOperation)toInt(platform::core::toNext(props,',', pos_b));
				BlendOperation BlendOpAlpha	=(BlendOperation)toInt(platform::core::toNext(props,',', pos_b));
				BlendOption SrcBlend			=(BlendOption)toInt(platform::core::toNext(props,',', pos_b));
				BlendOption DestBlend		=(BlendOption)toInt(platform::core::toNext(props,',', pos_b));
				BlendOption SrcBlendAlpha	=(BlendOption)toInt(platform::core::toNext(props,',', pos_b));
				BlendOption DestBlendAlpha	=(BlendOption)toInt(platform::core::toNext(props,',', pos_b));
				pos_b++;
				string maskstr= platform::core::toNext(props,')',pos_b);
				vector<string> ma= platform::core::split(maskstr,',');

				for(int i=0;i<desc.blend.numRTs;i++)
				{
					bool enable=toBool(en[i]);
					desc.blend.RenderTarget[i].blendOperation				=enable?BlendOp:BLEND_OP_NONE;
					desc.blend.RenderTarget[i].blendOperationAlpha			=enable?BlendOpAlpha:BLEND_OP_NONE;
					if(desc.blend.RenderTarget[i].blendOperation!=BLEND_OP_NONE)
					{
						desc.blend.RenderTarget[i].SrcBlend					=SrcBlend;
						desc.blend.RenderTarget[i].DestBlend				=DestBlend;
					}
					else
					{
						desc.blend.RenderTarget[i].SrcBlendAlpha			=BLEND_ONE;
						desc.blend.RenderTarget[i].DestBlendAlpha			=BLEND_ZERO;
					}
					if(desc.blend.RenderTarget[i].blendOperationAlpha!=BLEND_OP_NONE)
					{
						desc.blend.RenderTarget[i].SrcBlendAlpha			=SrcBlendAlpha;
						desc.blend.RenderTarget[i].DestBlendAlpha			=DestBlendAlpha;
					}
					else
					{
						desc.blend.RenderTarget[i].SrcBlendAlpha			=BLEND_ONE;
						desc.blend.RenderTarget[i].DestBlendAlpha			=BLEND_ZERO;
					}
					desc.blend.RenderTarget[i].RenderTargetWriteMask	=(i<(int)ma.size())?toInt(ma[i]):0xF;
				}
				RenderState *bs=renderPlatform->CreateRenderState(desc);
				blendStates[name]=bs;
			}
			else if (is_equal(word, "RenderTargetFormatState"))
			{
				RenderStateDesc desc;
				string name		 = words[1];
				desc.name			=name.c_str();
				desc.type		   = RTFORMAT;
				vector<string> props = platform::core::split(words[2], ',');
				if (props.size() != 8)
				{
					SIMUL_CERR << "Invalid number of formats for: " << name << std::endl;
				}
				props[0].erase(0,1);
				props[7].erase(1,1);
				for (int i = 0; i < 8; i++)
				{
					desc.rtFormat.formats[i] = (PixelOutputFormat)toInt(props[i]);
				}
				rtFormatStates[name]			= renderPlatform->CreateRenderState(desc);
			}
			else if(is_equal(word, "RasterizerState"))
			{
				string name		=words[1];
				RenderStateDesc desc;
				desc.name			=name.c_str();
				desc.type=RASTERIZER;
				/*
					RenderBackfaceCull (false,CULL_BACK,0,0,false,FILL_WIREFRAME,true,false,false,0)
					0 AntialiasedLineEnable
					1 cullMode
					2 DepthBias
					3 DepthBiasClamp
					4 DepthClipEnable
					5 fillMode
					6 FrontCounterClockwise
					7 MultisampleEnable
					8 ScissorEnable
					9 SlopeScaledDepthBias
				*/
				vector<string> props=platform::core::split(words[2],',');
				//desc.rasterizer.antialias		 	=toInt(props[0]);
				desc.rasterizer.cullFaceMode		=toCullFadeMode(props[1]);
				desc.rasterizer.frontFace		   	=toBool(props[6])?FRONTFACE_COUNTERCLOCKWISE:FRONTFACE_CLOCKWISE;
				desc.rasterizer.polygonMode		 	=toPolygonMode(props[5]);
				desc.rasterizer.polygonOffsetMode   =POLYGON_OFFSET_DISABLE;
				desc.rasterizer.viewportScissor	 	=toBool(props[8])?VIEWPORT_SCISSOR_ENABLE:VIEWPORT_SCISSOR_DISABLE;
				
				RenderState *bs	  =renderPlatform->CreateRenderState(desc);
				rasterizerStates[name]			  =bs;
			}
			else if(is_equal(word, "DepthStencilState"))
			{
				string name		=words[1];
				string props	=words[2];
				size_t pos_d		=0;
				RenderStateDesc desc;
				desc.name=name.c_str();
				desc.type=DEPTH;
				desc.depth.test=toBool(platform::core::toNext(props,',',pos_d));
				desc.depth.write=toInt(platform::core::toNext(props,',',pos_d))!=0;
				desc.depth.comparison=(DepthComparison)toInt(platform::core::toNext(props,',',pos_d));
				RenderState *ds=renderPlatform->CreateRenderState(desc);
				depthStencilStates[name]=ds;
			}
			else if(is_equal(word, "SamplerState"))
			{
				//SamplerState clampSamplerState 9,MIN_MAG_MIP_LINEAR,CLAMP,CLAMP,CLAMP,
				size_t sp2=line.find(" ",sp+1);
				string sampler_name = line.substr(sp + 1, sp2 - sp - 1);
				size_t comma=(int)std::min(line.length(),line.find(",",sp2+1));
				string register_num = line.substr(sp2 + 1, comma - sp2 - 1);
				int reg=atoi(register_num.c_str());
				SamplerStateDesc desc;
				string state=line.substr(comma+1,line.length()-comma-1);
				vector<string> st=platform::core::split(state,',');
				desc.filtering=stringToFilter(st[0]);
				desc.x=stringToWrapping(st[1]);
				desc.y=stringToWrapping(st[2]);
				desc.z=stringToWrapping(st[3]);
				if(st.size()>4)
					desc.depthComparison=(DepthComparison)toInt(st[4]);
				desc.slot=reg;
				SamplerState *ss=renderPlatform->GetOrCreateSamplerStateByName(sampler_name.c_str(),&desc);
				samplerStates[sampler_name]=ss;
				samplerSlots[reg]=ss;
				ShaderResource *res=new ShaderResource;
				res->slot				=reg;
				res->shaderResourceType	=ShaderResourceType::SAMPLER;
				textureDetailsMap[sampler_name]=res;
			}
		}
		else if (level == GROUP)
		{
			if (sp >= 0 && _stricmp(line.substr(0, sp).c_str(), "technique") == 0)
			{
				tech_name = line.substr(sp + 1, line.length() - sp - 1);
				tech = (EffectTechnique*)EnsureTechniqueExists(group_name, tech_name, "main");
				p=nullptr;
				passNum=0;
			}
		}
		else if (level == TECHNIQUE)
		{
			if(sp>=0&&_stricmp(line.substr(0,sp).c_str(),"pass")==0)
			{
				pass_name=line.substr(sp+1,line.length()-sp-1);
				p=(EffectPass*)tech->AddPass(pass_name.c_str(),passNum);
				shaderCount=0;
				layoutCount=0;
				layoutOffset=0;
				layoutSlot=0;
				passNum++;
			}
			if(sp>=0&&_stricmp(line.substr(0,sp).c_str(),"variant_pass")==0)
			{
				pass_name=line.substr(sp+1,line.length()-sp-1);
				variantPass=tech->AddVariantPass(pass_name.c_str());
				variant_mode=true;
			}
		}
		else if (level==VARIANT_PASS)
		{
			if(sp>=0&&_stricmp(line.substr(0,sp).c_str(),"variant")==0)
			{
				pass_name=line.substr(sp+1,line.length()-sp-1);
				p=(EffectPass*)tech->AddPass(pass_name.c_str(),passNum);
				variantPass->passes[pass_name]=p;
				shaderCount=0;
				layoutCount=0;
				layoutOffset=0;
				layoutSlot=0;
				passNum++;
			}
		}
		else if(level==LAYOUT)
		{
			std::regex element_re("([a-zA-Z]+[0-9]?)[\\s]+([a-zA-Z][a-zA-Z0-9_]*);");
			std::smatch sm;
			if(std::regex_search(line, sm, element_re))
			{
				std::string type=sm.str(1);
				std::string name=sm.str(2);
				LayoutDesc &desc=layoutDesc[layoutCount];
				desc.format				=TypeToFormat(type.c_str());
				desc.alignedByteOffset	=layoutOffset;
				desc.inputSlot			=layoutSlot;
				desc.perInstance		=false;
				// TODO: add semantic name/index to sfxo layouts.
				desc.semanticName		="";
				desc.semanticIndex		=0;
				layoutCount++;
				layoutOffset			+=GetByteSize(desc.format);
				layoutSlot++;
			}
		}
		else if(level==PASS||level==HITGROUP||level==MISS_SHADERS||level==CALLABLE_SHADERS||level==RAYTRACING_CONFIG)
		{
			if(sp>=0&&_stricmp(line.substr(0,sp).c_str(),"variant")==0)
			{
				pass_name=line.substr(sp+1,line.length()-sp-1);
				p=(EffectPass*)tech->AddPass(pass_name.c_str(),passNum);
				shaderCount=0;
				layoutCount=0;
				layoutOffset=0;
				layoutSlot=0;
				passNum++;
			}
			// Find the shader definitions e.g.:
			// vertex: simple_VS_Main_vv.sb
			// pixel: simple_PS_Main_p.sb

			size_t cl=line.find(":");
			size_t cm=line.find(",",cl+1);
			if(cm == std::string::npos)
				cm=line.length();
			if(cl != std::string::npos && tech)
			{
				string type=line.substr(0,cl);
				//string filename_entry=line.substr(cl+1,cm-cl-1);
				string uses;
				if(cm<line.length())
					uses=line.substr(cm+1,line.length()-cm-1);
				platform::core::ClipWhitespace(uses);
				platform::core::ClipWhitespace(type);
				//base::ClipWhitespace(filename_entry);

				std::regex re_file_entry("([a-z0-9A-Z_]+\\.[a-z0-9A-Z_]+)(?:\\(([a-z0-9A-Z_]+)\\))?(?:\\s*inline:\\(0x([a-f0-9A-F]+),0x([a-f0-9A-F]+)\\))?");
				std::smatch fe_smatch;
				
				std::smatch sm;
				string filenamestr;

				string entry_point="main";
				size_t inline_offset =0;
				size_t inline_length =0;

				if(std::regex_search(line, sm, re_file_entry))
				{
					filenamestr= sm.str(1);
					if(sm.length()>2)
						entry_point= sm.str(2);
					entry_point = sm.str(2);
					if (entry_point.length()>0&&sm.length() > 4)
					{
						if (sm.length(3) && sm.length(4))
						{
							string inline_offset_str = sm.str(3);
							string inline_length_str = sm.str(4);
							inline_offset = std::stoul(inline_offset_str, nullptr, 16);
							inline_length = std::stoul(inline_length_str, nullptr, 16);
							if (!bin_ptr)
							{
								platform::core::FileLoader::GetFileLoader()->AcquireFileContents(bin_ptr, bin_num_bytes, sfxbFilenameUtf8.c_str(), true);
								if (!bin_ptr)
								{
									SIMUL_BREAK(platform::core::QuickFormat("Failed to load combined shader binary: %s\n", sfxbFilenameUtf8.c_str()));
								}
							}
						}
					}
				}
				string name;
				if(words.size()>1)
					name=words[1];
				ShaderType t=ShaderType::SHADERTYPE_COUNT;
				PixelOutputFormat fmt=FMT_UNKNOWN;
				std::string passRtFormat = "";
				if(_stricmp(type.c_str(),"layout")==0)
				{
					layoutCount=0;
					layoutOffset=0;
					layoutSlot=0;
				}
				else if (_stricmp(type.c_str(), "numthreads") == 0)
				{
					if (words.size() > 1)
						p->numThreads.x=atoi(words[1].c_str());
					if (words.size() > 2)
						p->numThreads.y=atoi(words[2].c_str());
					if(words.size()>3)
						p->numThreads.z=atoi(words[3].c_str());
				}
				else if(_stricmp(type.c_str(),"blend")==0)
				{
					if(blendStates.find(name)!=blendStates.end())
					{
						p->blendState=blendStates[name];
					}
					else
					{
						SIMUL_CERR<<"State not found: "<<name<<std::endl;
					}
				}
				else if(_stricmp(type.c_str(),"rasterizer")==0)
				{
					if(rasterizerStates.find(name)!=rasterizerStates.end())
					{
						p->rasterizerState=rasterizerStates[name];
					}
					else
					{
						SIMUL_CERR<<"Rasterizer state not found: "<<name<<std::endl;
					}
				}
				else if (_stricmp(type.c_str(), "targetformat") == 0)
				{
					if (rtFormatStates.find(name) != rtFormatStates.end())
					{
						p->renderTargetFormatState = rtFormatStates[name];
					}
					else
					{
						SIMUL_CERR << "Render Target Format state not found: " << name << std::endl;
					}
				}
				else if(_stricmp(type.c_str(),"depthstencil")==0)
				{
					if(depthStencilStates.find(name)!=depthStencilStates.end())
					{
						p->depthStencilState=depthStencilStates[name];
					}
					else
					{
						SIMUL_CERR<<"Depthstencil state not found: "<<name<<std::endl;
					}
				}
				else if(_stricmp(type.c_str(),"topology")==0)
				{
					Topology t=toTopology(name);
					p->SetTopology(t);
				}
				else if (_stricmp(type.c_str(), "multiview") == 0)
				{
					p->multiview = name.compare("1") == 0;
				}
				else
				{
					if(_stricmp(type.c_str(),"vertex")==0)
						t=SHADERTYPE_VERTEX;
					else if(_stricmp(type.c_str(),"export")==0)
						t=SHADERTYPE_VERTEX;
					else if(_stricmp(type.c_str(),"geometry")==0)
						t=SHADERTYPE_GEOMETRY;
					else if(_stricmp(type.substr(0,5).c_str(),"pixel")==0)
					{
						t=SHADERTYPE_PIXEL;
						if(type.length()>6)
						{
							string out_fmt=type.substr(6,type.length()-7);
							if(_stricmp(out_fmt.c_str(),"float16abgr")==0)
								fmt=FMT_FP16_ABGR;
							else if(_stricmp(out_fmt.c_str(),"float32abgr")==0)
								fmt=FMT_32_ABGR;
							else if(_stricmp(out_fmt.c_str(),"snorm16abgr")==0)
								fmt=FMT_SNORM16_ABGR;
							else if(_stricmp(out_fmt.c_str(),"unorm16abgr")==0)
								fmt=FMT_UNORM16_ABGR;
							else
							{
								// Handle rt format state
								size_t pb = type.find("(");
								size_t pe = type.find(")");
								if (pe - pb > 1)
								{
									passRtFormat = std::string(type.begin() + pb, type.begin() + pe);
								}
							}
						}
					}
					else if(_stricmp(type.c_str(),"compute")==0)
						t=SHADERTYPE_COMPUTE;
					else if(_stricmp(type.c_str(),"raygeneration")==0)
						t=SHADERTYPE_RAY_GENERATION;
					else if(_stricmp(type.c_str(),"hitgroup")==0)
					{
						level=HITGROUP;
						hg=&p->raytraceHitGroups[name];
						next	=(int)str.find('\n',pos+1);
						continue;
					}
					else if (_stricmp(type.c_str(), "MissShaders") == 0)
					{
						level = MISS_SHADERS;
						next = (int)str.find('\n', pos + 1);
						continue;
					}
					else if (_stricmp(type.c_str(), "CallableShaders") == 0)
					{
						level = CALLABLE_SHADERS;
						next = (int)str.find('\n', pos + 1);
						continue;
					}
					else if (_stricmp(type.c_str(), "RayTracingShaderConfig") == 0)
					{
						level = RAYTRACING_CONFIG;
						next = (int)str.find('\n', pos + 1);
						continue;
					}
					else if (_stricmp(type.c_str(), "RayTracingPipelineConfig") == 0)
					{
						level = RAYTRACING_CONFIG;
						next = (int)str.find('\n', pos + 1);
						continue;
					}
					else if(_stricmp(type.c_str(),"miss")==0)
						t=SHADERTYPE_MISS;
					else if(_stricmp(type.c_str(),"callable")==0)
						t=SHADERTYPE_CALLABLE;
					else if(_stricmp(type.c_str(),"closesthit")==0)
						t=SHADERTYPE_CLOSEST_HIT;
					else if(_stricmp(type.c_str(),"anyhit")==0)
						t=SHADERTYPE_ANY_HIT;
					else if(_stricmp(type.c_str(),"intersection")==0)
						t=SHADERTYPE_INTERSECTION;
					else if (_stricmp(type.c_str(), "maxpayloadsize") == 0)
					{
						std::string str_num = line.substr(std::string("maxpayloadsize: ").size());
						p->maxPayloadSize = atoi(str_num.c_str());
					}
					else if(_stricmp(type.c_str(),"maxattributesize")==0)
					{
						std::string str_num = line.substr(std::string("maxattributesize: ").size());
						p->maxAttributeSize = atoi(str_num.c_str());
					}
					else if(_stricmp(type.c_str(),"maxtracerecursiondepth")==0)
					{
						std::string str_num = line.substr(std::string("maxtracerecursiondepth: ").size());
						p->maxTraceRecursionDepth = atoi(str_num.c_str());
					}
					else
					{
						SIMUL_BREAK(platform::core::QuickFormat("Unknown shader type or command: %s\n",type.c_str()));
						continue;
					}
					Shader *s = nullptr;
					if (bin_ptr)
					{
						s=renderPlatform->EnsureShader(filenamestr.c_str(), bin_ptr, inline_offset, inline_length, t);
					}
					else if(filenamestr.length())
					{
						s=renderPlatform->EnsureShader(filenamestr.c_str(), t);
					}
					if(s)
					{
						if(t==SHADERTYPE_PIXEL&&fmt!=FMT_UNKNOWN)
							p->pixelShaders[fmt]=s;
						else
						{
							if(level==PASS)
							{
								p->shaders[t]=s;
							}
							else if(level==HITGROUP||level==MISS_SHADERS||level==CALLABLE_SHADERS)
							{
								if(t==SHADERTYPE_CLOSEST_HIT)
								{
									hg->closestHit=s;
								}
								if(t==SHADERTYPE_ANY_HIT)
								{
									hg->anyHit=s;
								}
								if(t==SHADERTYPE_INTERSECTION)
								{
									hg->intersection=s;
								}
								if(t==SHADERTYPE_MISS)
								{
									s->entryPoint=entry_point;
									p->missShaders[s->entryPoint]=s;
								}
								if(t==SHADERTYPE_CALLABLE)
								{
									s->entryPoint=entry_point;
									p->callableShaders[s->entryPoint]=s;
								}
							}
							else
							{
							}
						}
						if (!passRtFormat.empty())
						{
							p->rtFormatState = passRtFormat;
						}
						shaderCount++;
					}
					else if(filenamestr.length()>0)
					{
						SIMUL_BREAK_ONCE(platform::core::QuickFormat("Failed to load shader %s",filenamestr.c_str()));
					}
					// Set what the shader uses.

					// textures,buffers,samplers
					std::regex re_resources("([a-z]):\\(([^\\)]*)\\)");
					std::smatch res_smatch;

					auto i_begin =std::sregex_iterator(uses.begin(),uses.end(),re_resources);
					auto i_end = std::sregex_iterator();
					Slots cbSlots = {}, shaderSamplerSlots = {}, 
						textureSlots = {}, rwTextureSlots = {}, 
						textureSlotsForSB = {}, rwTextureSlotsForSB = {};

					for(std::sregex_iterator i = i_begin; i != i_end; ++i)
					{
						string mtch= i->str(0);
						string type_letter = i->str(1);
						char type_char		=type_letter[0];
						string spec			=i->str(2);
						smatch res;
						std::regex re("([0-9]+)");
						std::smatch match;
						if(std::regex_search(spec,match,re))
						{
							auto j_begin = std::sregex_iterator(spec.begin(), spec.end(), re);
							for (std::sregex_iterator j = j_begin; j != i_end; ++j)
							{
								uint64_t u = atoi(j->str().c_str());
								if (type_char == 'c')
								{
									cbSlots.set(u);
								}
								else if (type_char == 's')
								{
									shaderSamplerSlots.set(u);
								}
								else if (type_char == 't')
								{
									if (u < 1000)
										textureSlots.set(u);
									else
										rwTextureSlots.set(u - 1000);
								}
								else if (type_char == 'u')
								{
									rwTextureSlots.set(u);
								}
								else if (type_char == 'b')
								{
									if (u < 1000)
										textureSlotsForSB.set(u);
									else
										rwTextureSlotsForSB.set(u - 1000);
								}
								else if (type_char == 'z')
								{
									rwTextureSlotsForSB.set(u);
								}
								else
								{
									SIMUL_CERR << "Unknown resource letter " << type_char << std::endl;
								}
							}
						}
					}
					if(s)
					{
						if (s->constantBufferSlots.none())
							s->constantBufferSlots	= cbSlots;
						if (s->textureSlots.none())
							s->textureSlots			= textureSlots;
						if (s->samplerSlots.none())
							s->samplerSlots			= shaderSamplerSlots;
						if (s->rwTextureSlots.none())
							s->rwTextureSlots		= rwTextureSlots;
						if (s->textureSlotsForSB.none())
							s->textureSlotsForSB	= textureSlotsForSB;
						if (s->rwTextureSlotsForSB.none())
							s->rwTextureSlotsForSB	= rwTextureSlotsForSB;
						// Now we will know which slots must be used by the pass:
						p->SetUsesTextureSlots(s->textureSlots);
						p->SetUsesTextureSlotsForSB(s->textureSlotsForSB);
						p->SetUsesRwTextureSlots(s->rwTextureSlots);
						p->SetUsesRwTextureSlotsForSB(s->rwTextureSlotsForSB);
						p->SetUsesSamplerSlots(s->samplerSlots);
						p->SetUsesConstantBufferSlots(s->constantBufferSlots);

						// set the actual sampler states for each shader based on the slots it uses:
						// Which sampler states are needed?
						Slots slots = s->samplerSlots;
						for (int slot = 0; slot < s->samplerSlots.size(); slot++)
						{
							if (slots[slot])
							{
								for (auto j : samplerStates)
								{
									if (samplerSlots[slot] == j.second)
									{
										std::string ss_name = j.first;
										SamplerState *ss = renderPlatform->GetOrCreateSamplerStateByName(ss_name.c_str());
										s->samplerStates[slot] = ss;
									}
								}
							}
							s->samplerSlots.set(slot, false);
							if(slots.none())
								break;
						}
						p->MakeResourceSlotMap();
						s->entryPoint=entry_point;
						if(t==SHADERTYPE_VERTEX&&layoutCount)
						{
							s->layout.SetDesc(layoutDesc,layoutCount);
							layoutCount=0;
						}
					}
				}
			}
		}
		size_t close_brace=line.find("}");
		if (close_brace != std::string::npos)
		{
			if(level==PASS)
			{
				if(shaderCount==0)
				{
					SIMUL_CERR<<"No shaders in pass "<<pass_name.c_str()<<" of effect "<<filename_utf8<<std::endl;
				}
			}
			if(level==HITGROUP||level==MISS_SHADERS||level==CALLABLE_SHADERS||level==RAYTRACING_CONFIG)
				level=PASS;
			else
			{
				level = (Level)(level - 1);
				if(level==VARIANT_PASS&&!variant_mode)
					level=TECHNIQUE;
			}
			if(level==TECHNIQUE)
			{
				variantPass=nullptr;
				variant_mode=false;
			}
			if (level == OUTSIDE)
				group_name = "";
		}
		next	=(int)str.find('\n',pos+1);
	}
	SIMUL_ASSERT(level==OUTSIDE);
	platform::core::FileLoader::GetFileLoader()->ReleaseFileContents(ptr);
	if (bin_ptr)
		platform::core::FileLoader::GetFileLoader()->ReleaseFileContents(bin_ptr);
	PostLoad();

	return true;
}

const ShaderResource *Effect::GetShaderResourceAtSlot(int s) 
{
	return textureResources[s];
}
EffectTechniqueGroup *Effect::GetTechniqueGroupByName(const char *name)
{
	auto i=groupCharMap.find(name);
	if(i!=groupCharMap.end())
		return i->second;
	auto j=groups.find(name);
	if(j!=groups.end())
		return j->second;
	return nullptr;
}
EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	if(!groupCharMap.size())
	{
		SIMUL_CERR_ONCE << "groupCharMap size was 0 when getting technique: " << name << ".\n";
		return nullptr;
	}
	return groupCharMap[0]->GetTechniqueByName(name);
}
ShaderResource Effect::GetShaderResource(const char *name)
{
	ShaderResource res;
	auto i = GetTextureDetails(name);
	unsigned slot=0xFFFFFFFF;
	if (i)
	{
		slot					=GetSlot(name);
		res.valid				=true;
		unsigned dim					=i->dimensions;//GetDimensions(name);
		res.dimensions					=dim;
		res.shaderResourceType			=i->shaderResourceType;//(name);
	}
	else
	{
		int s=GetSamplerStateSlot(name);
		if(s<0)
		{
			res.valid = false;
			SIMUL_CERR_ONCE << "Invalid Shader resource name: " << (name ? name : "") << std::endl;
			return res;
		}
		slot=s;
		res.shaderResourceType		=ShaderResourceType::SAMPLER;
	}
	 
	res.platform_shader_resource	=(void*)nullptr;
	res.slot						=slot;
	return res;
}

void Effect::SetTexture(DeviceContext& deviceContext, const ShaderResource& res, Texture* tex, const SubresourceRange& subresource)
{
	renderPlatform->SetTexture(deviceContext, res, tex, subresource);
}
void Effect::SetTexture(DeviceContext& deviceContext, const char* name, Texture* tex, const SubresourceRange& subresource)
{
	const ShaderResource& i = GetShaderResource(name);
	SetTexture(deviceContext, i, tex, subresource);
}

void Effect::SetUnorderedAccessView(DeviceContext& deviceContext, const ShaderResource& res, Texture* tex, const SubresourceLayers& subresource)
{
	renderPlatform->SetUnorderedAccessView(deviceContext, res, tex, subresource);
}
void Effect::SetUnorderedAccessView(DeviceContext& deviceContext, const char* name, Texture* t, const SubresourceLayers& subresource)
{
	const ShaderResource& i = GetShaderResource(name);
	SetUnorderedAccessView(deviceContext, i, t, subresource);
}

void Effect::SetSamplerState(DeviceContext &deviceContext,const ShaderResource &shaderResource,SamplerState *s)
{
	if(shaderResource.slot>31||shaderResource.slot<0)
		return;
	ContextState *cs = renderPlatform->GetContextState(deviceContext);

	cs->samplerStateOverrides[shaderResource.slot] = s;
	cs->samplerStateOverridesValid = false;
}
void Effect::SetConstantBuffer(DeviceContext &deviceContext,ConstantBufferBase *s)
{
	renderPlatform->SetConstantBuffer(deviceContext,s);
}

void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,const char *pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}
void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,int pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}
void Effect::Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,int pass_num)
{
	if(effectTechnique)
	{
		EffectPass *p				=(effectTechnique)->GetPass(pass_num>=0?pass_num:0);
		deviceContext.renderPlatform->ApplyPass(deviceContext, p);
	}
}
void Effect::Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,const char *passname)
{
	EffectPass* p = nullptr;
	if (effectTechnique)
	{
		if(passname)
			p=effectTechnique->GetPass(passname);
		else
			p=effectTechnique->GetPass(0);
	}
	Apply(deviceContext,p);
}
void Effect::Apply(DeviceContext &deviceContext, EffectPass* p)
{
	renderPlatform->ApplyPass(deviceContext, p);
}

void Effect::Reapply(DeviceContext& deviceContext)
{
	auto *p= deviceContext.contextState.currentEffectPass;
	Unapply(deviceContext);
	Apply(deviceContext,p);
}
void Effect::Unapply(DeviceContext &deviceContext)
{
	renderPlatform->UnapplyPass(deviceContext);
}
void Effect::UnbindTextures(DeviceContext &deviceContext)
{
	if(!renderPlatform)
		return;
	ContextState *cs=renderPlatform->GetContextState(deviceContext);
	cs->textureAssignmentMap.clear();
	cs->rwTextureAssignmentMap.clear();
	cs->samplerStateOverrides.clear();
	cs->applyBuffers.clear();  //Because we might otherwise have invalid data
	cs->applyVertexBuffers.clear();
	cs->indexBuffer = nullptr;
	cs->invalidate();
	// Clean up the TextureAssignments (used in some platforms a.k.a. Switch and Opengl)
}

void Effect::StoreConstantBufferLink(ConstantBufferBase *b)
{
	linkedConstantBuffers.insert(b);
}
bool Effect::IsLinkedToConstantBuffer(ConstantBufferBase*b) const
{
	return (linkedConstantBuffers.find(b)!=linkedConstantBuffers.end());
}

int Effect::GetSlot(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return -1;
	return i->slot;
}
int Effect::GetSamplerStateSlot(const char *name)
{
	auto i=samplerStates.find(std::string(name));
	if(i==samplerStates.end())
		return -1;
	return i->second->default_slot;
}
int Effect::GetDimensions(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return -1;
	return i->dimensions;
}
ShaderResourceType Effect::GetResourceType(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return ShaderResourceType::COUNT;
	return i->shaderResourceType;
}

EffectTechnique *Effect::EnsureTechniqueExists(const string &groupname,const string &techname_,const string &passname)
{
	EffectTechnique *tech=nullptr;
	string techname=techname_;
	{
		if(groups.find(groupname)==groups.end())
		{
			groups[groupname]=new EffectTechniqueGroup;
			if(groupname.length()==0)
				groupCharMap[nullptr]=groups[groupname];
		}
		EffectTechniqueGroup *group=groups[groupname];
		if(group->techniques.find(techname)!=group->techniques.end())
			tech=group->techniques[techname];
		else
		{
			tech								=CreateTechnique(); 
			int index							=(int)group->techniques.size();
			group->techniques[techname]			=tech;
			group->techniques_by_index[index]	=tech;
			if(groupname.size())
				techname=(groupname+"::")+techname;
			techniques[techname]			=tech;
			techniques_by_index[index]		=tech;
			tech->name						=techname;
		}
	}
	return tech;
}
const char *Effect::GetTechniqueName(const EffectTechnique *t) const
{
	for(auto g=groups.begin();g!=groups.end();g++)
	for(EffectTechniqueGroup::TechniqueMap::const_iterator i=g->second->techniques.begin();i!=g->second->techniques.end();i++)
	{
		if(i->second==t)
			return i->first.c_str();
	}
	return "";
}
const ShaderResource *Effect::GetTextureDetails(const char *name)
{
	auto j=textureCharMap.find(name);
	if(j!=textureCharMap.end())
		return j->second;
	for(const auto& i:textureDetailsMap)
	{
		if(strcmp(i.first.c_str(),name)==0)
		{
			textureCharMap.insert({ i.first.c_str(),i.second });
			return i.second;
		}
	}
	return nullptr;
}

//////////////////////
//ConstantBufferBase//
//////////////////////

ConstantBufferBase::ConstantBufferBase(const char *name) 
	:platformConstantBuffer(nullptr)
{
	if (name&&strlen(name) >7)
		defaultName = name + 7;
}