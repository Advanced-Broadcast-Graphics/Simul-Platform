#define NOMINMAX
#include "Texture.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/FileLoader.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include <string>
#include <algorithm>
#include <DirectXTex.h>

using namespace simul;
using namespace dx12;

SamplerState::SamplerState(crossplatform::SamplerStateDesc *d):
    mCachedDesc(*d)
{
}

SamplerState::~SamplerState()
{
	InvalidateDeviceObjects();
}

void SamplerState::InvalidateDeviceObjects()
{
}

Texture::Texture():
	mTextureUpload(nullptr),
	mTextureDefault(nullptr),
	layerShaderResourceViews12(nullptr),
	mainMipShaderResourceViews12(nullptr),
	layerMipShaderResourceViews12(nullptr),
	mipUnorderedAccessViews12(nullptr),
	layerMipUnorderedAccessViews12(nullptr),
	renderTargetViews12(nullptr),
	mLoadedFromFile(false),
    mNumSamples(1)
	,loadedData(nullptr)
	,metadata(nullptr)
	,scratchImage(nullptr)
{
	// Set the pointer to an invalid value so we can perform checks
	mainShaderResourceView12.ptr	= -1;
	arrayShaderResourceView12.ptr	= -1;
	depthStencilView12.ptr			= -1;
}

Texture::~Texture()
{
	delete metadata;
	delete scratchImage;
	InvalidateDeviceObjects();
}

void Texture::FreeRTVTables()
{
	if (renderTargetViews12)
	{
		int total_num = cubemap ? arraySize * 6 : arraySize;
		for (int i = 0; i<total_num; i++)
		{
			delete[] renderTargetViews12[i];
		}
		delete[] renderTargetViews12;
		renderTargetViews12 = nullptr;
	}
}

void Texture::InitUAVTables(int l,int m)
{
	InitStateTable(l, m);

	mipUnorderedAccessViews12 = nullptr;
	if (m)
		mipUnorderedAccessViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE[m];		// UAV's for whole texture at different mips.

	layerMipUnorderedAccessViews12 = nullptr;
	if (l&&m)
	{
		layerMipUnorderedAccessViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE*[l];
		for (int i = 0; i<l; i++)
		{
			layerMipUnorderedAccessViews12[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[m];	// UAV's for each layer at different mips.
		}
	}
}

void Texture::FreeUAVTables()
{
	if (mipUnorderedAccessViews12)
	{
		delete[] mipUnorderedAccessViews12;
	}
	mipUnorderedAccessViews12 = nullptr;

	if (layerMipShaderResourceViews12)
	{
		int total_num = cubemap ? arraySize * 6 : arraySize;
		for (int i = 0; i<total_num; i++)
		{
			delete[] layerMipShaderResourceViews12[i];
		}
		delete[] layerMipShaderResourceViews12;
		layerMipShaderResourceViews12 = nullptr;
	}
}

void Texture::InitRTVTables(int l,int m)
{
	InitStateTable(l, m);

	renderTargetViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE*[l];
	for (int i = 0; i<l; i++)
	{
		renderTargetViews12[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[m];
	}
}

void Texture::InitStateTable(int l, int m)
{
	// Create state table, at this point, we expect that the state
	// has already being set !!!
	auto curState = GetCurrentState();
	mSubResourcesStates.clear();
	mSubResourcesStates.resize(l);
	for (int layer = 0; layer < l; layer++)
	{
		mSubResourcesStates[layer].resize(m);
		for (int mip = 0; mip < m; mip++)
		{
			mSubResourcesStates[layer][mip] = curState;
		}
	}
}

void Texture::InvalidateDeviceObjects()
{
	FreeRTVTables();
	FreeUAVTables();
	FreeSRVTables();

	// Set the pointer to an invalid value so we can perform checks
	mainShaderResourceView12.ptr	= -1;
	arrayShaderResourceView12.ptr	= -1;
	depthStencilView12.ptr			= -1;

	arraySize				= 0;
	mips					= 0;

	auto rPlat = (dx12::RenderPlatform*)(renderPlatform);
	// We dont want to release the resources of the external texture
	if (!mInitializedFromExternal)
	{
		// Critical resources like textures that could be in use by the GPU will be destroyed by the 
		// release manager
		if(mTextureDefault)
			rPlat->PushToReleaseManager(mTextureDefault, name + "_Default");
		if(mTextureUpload)
			rPlat->PushToReleaseManager(mTextureUpload, name + "_Upload");
	}

	mTextureDefault=nullptr;
	mTextureUpload=nullptr;
	// Same for heaps (The release method will handle it)
	mTextureSrvHeap.Release();
	mTextureUavHeap.Release();
	mTextureRtHeap.Release();
	mTextureDsHeap.Release();
}

void Texture::LoadFromFile(crossplatform::RenderPlatform *renderPlatform,const char *pFilePathUtf8)
{
	const std::vector<std::string> &pathsUtf8=renderPlatform->GetTexturePathsUtf8();
	InvalidateDeviceObjects();

	HRESULT res = S_FALSE;

	// Set initial properties of the texture
	name					= pFilePathUtf8;
	mips					= 1;
	arraySize				= 1;
	depth					= 1;
	cubemap					= false;
	this->renderPlatform	= renderPlatform;

	// Find the path to the file
	std::string strPath;
	int idx = simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(pFilePathUtf8, pathsUtf8);
	if (idx<-1 || idx >= (int)pathsUtf8.size())
		return;
	strPath = pFilePathUtf8;
	if (idx >= 0)
		strPath = (pathsUtf8[idx] + "/") + strPath;
	 name=pFilePathUtf8;

	// Load the data
	unsigned bytes	= 0;
	int flags		= DirectX::WIC_FLAGS_NONE;
	simul::base::FileLoader::GetFileLoader()->AcquireFileContents(loadedData, bytes, strPath.c_str(), false);
	
	if(!metadata)
		metadata=new DirectX::TexMetadata();
	if(!scratchImage)
		scratchImage=new DirectX::ScratchImage;
	if(name.find(".hdr")==name.length()-4)
	{
		res= DirectX::LoadFromHDRMemory( loadedData, _In_ bytes,metadata,*scratchImage );

	}
    else if (name.find(".dds") != std::string::npos)
    {
        res = DirectX::LoadFromDDSMemory(loadedData, bytes, flags,metadata, *scratchImage);
    }
    else
    {
	    res = DirectX::LoadFromWICMemory(loadedData, bytes, flags, metadata, *scratchImage);
    }
    SIMUL_ASSERT(res == S_OK)
		
	// The texture will be considered "valid" from here.
	// Set the properties of this texture
	width			= (int)metadata->width;
	length			= (int)metadata->height;
	pixelFormat		=RenderPlatform::FromDxgiFormat(metadata->format);
	mLoadedFromFile = true;
	
	// ok to free loaded data??
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(loadedData);
	textureLoadComplete=false;
}

void Texture::FinishLoading(crossplatform::DeviceContext &deviceContext)
{
	if(textureLoadComplete)
		return;
	// Convert into WIC 
	const DirectX::Image*	image;
	HRESULT res=S_OK;
	image   = scratchImage->GetImage(0, 0, 0);
	 
	if (!image)
	{
		SIMUL_BREAK("Failed to load this texture.");
		return;
	}

	// Make the texture description
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment			= 0;							// Let runtime decide
	textureDesc.Width				= (UINT)metadata->width;
	textureDesc.Height				= (UINT)metadata->height;
	textureDesc.DepthOrArraySize	= 1;
	textureDesc.MipLevels			= 1;
	textureDesc.Format				= metadata->format;
	textureDesc.SampleDesc.Count	= 1;
	textureDesc.SampleDesc.Quality	= 0;
	textureDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN; // Let runtime decide
	textureDesc.Flags				= D3D12_RESOURCE_FLAG_NONE;

	// Clear resources
	auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
	renderPlatformDx12->PushToReleaseManager(mTextureDefault, "mTextureDefault");
	renderPlatformDx12->PushToReleaseManager(mTextureUpload, "mTextureUpload");
	mTextureDefault = nullptr;
	mTextureUpload = nullptr;

	// Create the texture resource (GPU, with an implicit heap)
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
        SIMUL_PPV_ARGS(&mTextureDefault)
	);
	SIMUL_ASSERT(res == S_OK);
	D3D12_GPU_VIRTUAL_ADDRESS va=mTextureDefault->GetGPUVirtualAddress();
	std::wstring	n = L"GPU_";
	n+=std::wstring(name.begin(), name.end());
	mTextureDefault->SetName(n.c_str());

	// Create an upload heap
	UINT64 textureUploadBufferSize = 0;
	renderPlatform->AsD3D12Device()->GetCopyableFootprints
	(
		&textureDesc,
		0,1,0,
		nullptr,nullptr,nullptr,
		&textureUploadBufferSize
	);
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
        SIMUL_PPV_ARGS(&mTextureUpload)
	);
	SIMUL_ASSERT(res == S_OK);

	SIMUL_GPU_TRACK_MEMORY(mTextureUpload, textureUploadBufferSize)
	SIMUL_GPU_TRACK_MEMORY(mTextureDefault, textureUploadBufferSize)
	n = L"UPLOAD_";
	n+= std::wstring(name.begin(), name.end());
	mTextureUpload->SetName(n.c_str());

	// Perform the texture copy
	D3D12_SUBRESOURCE_DATA textureSubData	= {};
	textureSubData.pData					= image->pixels; 
	textureSubData.RowPitch					= image->rowPitch; 
	textureSubData.SlicePitch				= image->rowPitch * textureDesc.Height;

	// TODO: can't do this here, no valid command list!
	UpdateSubresources(renderPlatform->AsD3D12CommandList(), mTextureDefault, mTextureUpload, 0, 0, 1, &textureSubData);

	// Init states
	InitStateTable(arraySize, mips);

	// Transition the resource to be used in the shaders
	AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
	auto rPlat		= (dx12::RenderPlatform*)(renderPlatform);
	rPlat->ResourceTransitionSimple(mTextureDefault, D3D12_RESOURCE_STATE_COPY_DEST, GetCurrentState(),true);

	// Create a descriptor heap for this texture
	// This heap won't be shader visible as we will be copying the descriptor from here to the FrameHeap (which is shader visible)
	mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,"TextureCpuHeap",false);

	// Create the descriptor (view) of this texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format							= textureDesc.Format;
	srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels				= 1;
	rPlat->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.GetCpuHandleFromStartAfOffset(0));

	// Create the handle
	mainShaderResourceView12 = mTextureSrvHeap.GetCpuHandleFromStartAfOffset(0);
	textureLoadComplete=true;
}

void Texture::LoadTextureArray(crossplatform::RenderPlatform *r,const std::vector<std::string> &texture_files,int m)
{
	const std::vector<std::string> &pathsUtf8=r->GetTexturePathsUtf8();
	InvalidateDeviceObjects();

	HRESULT res = S_FALSE;

	// Set initial properties of the texture
	name			= "Array:" + texture_files[0] + ",...";
	mips			= m;
	arraySize		= (int)texture_files.size();
	depth			= (int)texture_files.size();
	cubemap			= false;
	renderPlatform	= r;
	// Find all the paths
	std::vector<std::string> strPaths;
	strPaths.resize(arraySize);
	for (int i = 0; i < arraySize; i++)
	{
		int idx = simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(texture_files[i].c_str(), pathsUtf8);
		if (idx<-1 || idx >= (int)pathsUtf8.size())
			return;
		strPaths[i] = texture_files[i];
		if (idx >= 0)
			strPaths[i] = (pathsUtf8[idx] + "/") + strPaths[i];
	}

	// Load the data
	struct FileContents
	{
		void* ptr		= NULL;
		unsigned bytes	= 0;
		int flags		= 0;
	};
	std::vector<FileContents> fileContents;
	fileContents.resize(arraySize);
	for (int i = 0; i < arraySize; i++)
	{
		auto curContents = &fileContents[i];
		simul::base::FileLoader::GetFileLoader()->AcquireFileContents(curContents->ptr, curContents->bytes, strPaths[i].c_str(), false);
	}
	// Convert into WIC 
	struct WicContents
	{
		DirectX::TexMetadata	metadata;
		DirectX::ScratchImage	scratchImage;
		const DirectX::Image*	image;
	};
	std::vector<WicContents> wicContents;
	wicContents.resize(arraySize);
	for (int i = 0; i < arraySize;i++)
	{
		auto curContents	= &fileContents[i];
		auto curWic			= &wicContents[i];
        if (texture_files[0].find(".dds") != std::string::npos)
        {
            res = DirectX::LoadFromDDSMemory(curContents->ptr, curContents->bytes, curContents->flags, &curWic->metadata, curWic->scratchImage);
        }
        else
        {
            res = DirectX::LoadFromWICMemory(curContents->ptr, curContents->bytes, curContents->flags, &curWic->metadata, curWic->scratchImage);
        }
		curWic->image = curWic->scratchImage.GetImage(0, 0, 0);
	}
	// Check that formats and sizes are the same (we will use as reference the first image in the array)
	unsigned int	mainWidth	= (unsigned int)wicContents[0].metadata.width;
	unsigned int	mainLength	= (unsigned int)wicContents[0].metadata.height;
	DXGI_FORMAT		mainFormat	= wicContents[0].metadata.format;
	for (int i = 1; i < arraySize; i++)
	{
		if (wicContents[i].metadata.width != mainWidth ||
			wicContents[i].metadata.width != mainLength ||
			wicContents[i].metadata.format != mainFormat)
		{
			SIMUL_BREAK("The textures provided don't have the same properties");
			return;
		}
	}
	
	auto format = RenderPlatform::FromDxgiFormat(mainFormat);

	// Clean resources
	auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
	renderPlatformDx12->PushToReleaseManager(mTextureDefault, "mTextureDefault");
	renderPlatformDx12->PushToReleaseManager(mTextureUpload, "mTextureUpload");
	mTextureDefault = nullptr;
	mTextureUpload = nullptr;

	ensureTextureArraySizeAndFormat(r,(int)wicContents[0].metadata.width,(int)wicContents[0].metadata.height,arraySize,m,format,false,true);
	
	// Make the texture description
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment			= 0;							// Let runtime decide
	textureDesc.Width				= mainWidth;
	textureDesc.Height				= mainLength;
	textureDesc.DepthOrArraySize	= arraySize;
	textureDesc.MipLevels			= m;
	textureDesc.Format				= mainFormat;
	textureDesc.SampleDesc.Count	= 1;
	textureDesc.SampleDesc.Quality	= 0;
	textureDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN; // Let runtime decide
	textureDesc.Flags				= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	textureDesc=mTextureDefault->GetDesc();
	std::string sName = name;
	std::wstring n = L"GPU_";
	n += std::wstring(sName.begin(), sName.end());
	mTextureDefault->SetName(n.c_str());
	
	// Create an upload heap
	UINT64 sliceSize [16];
	UINT64 textureUploadBufferSize = 0;
	UINT   numRows[16];
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT pLayouts[16];
	// only single mip in the upload.
	textureDesc.MipLevels=1;
	r->AsD3D12Device()->GetCopyableFootprints
	(
		&textureDesc,
		0, arraySize, 0,
		pLayouts, numRows, sliceSize,
		&textureUploadBufferSize
	);
	res = r->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ
		,nullptr,
        SIMUL_PPV_ARGS(&mTextureUpload)
	);
	SIMUL_ASSERT(res == S_OK);
	SIMUL_GPU_TRACK_MEMORY(mTextureUpload, textureUploadBufferSize)
	n = L"UPLOAD_";
	n += std::wstring(sName.begin(), sName.end());
	mTextureUpload->SetName(std::wstring(sName.begin(), sName.end()).c_str());
	
	// Perform the texture copy
	std::vector<D3D12_SUBRESOURCE_DATA> textureSubDatas;
	textureSubDatas.resize(arraySize);
	dx12::RenderPlatform *dx12RenderPlatform		= (dx12::RenderPlatform*)renderPlatform;
	for (int i = 0; i < arraySize; i++)
	{
		textureSubDatas[i].pData = wicContents[i].image->pixels;
		textureSubDatas[i].RowPitch = wicContents[i].image->rowPitch;
		textureSubDatas[i].SlicePitch = wicContents[i].image->rowPitch * textureDesc.Height;
		dx12RenderPlatform->ResourceTransitionSimple(mTextureDefault, GetCurrentState(), D3D12_RESOURCE_STATE_COPY_DEST, true);
		UpdateSubresources(r->AsD3D12CommandList(), mTextureDefault, mTextureUpload,pLayouts[i].Offset, i*m, 1, &textureSubDatas[i]);
		dx12RenderPlatform->ResourceTransitionSimple(mTextureDefault, D3D12_RESOURCE_STATE_COPY_DEST, GetCurrentState(), true);
	}
	
	mLoadedFromFile = true;
	
	// Clean!
	for (int i = 0; i < arraySize; i++)
	{
		simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(fileContents[i].ptr);
	}
}

bool Texture::IsValid() const
{
	if (mTextureDefault != NULL)
		return true;
	if(mLoadedFromFile&&!textureLoadComplete)
		return true;
	return false;
}

ID3D12Resource* Texture::AsD3D12Resource()
{
	return mTextureDefault;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12ShaderResourceView(bool setState /*= true*/, crossplatform::ShaderResourceType t, int index, int mip)
{
    if (mip >= mips)
    {
        mip = 0;
    }
	if(!textureLoadComplete)
	{
		FinishLoading(renderPlatform->GetImmediateContext());
		textureLoadComplete=true;
	}
#if 0
	if(!subResourcesUpdated)
	{
		// Perform the texture copy
		D3D12_SUBRESOURCE_DATA textureSubData	= {};
		textureSubData.pData					= image->pixels; 
		textureSubData.RowPitch					= image->rowPitch; 
		textureSubData.SlicePitch				= image->rowPitch * textureDesc.Height;
		UpdateSubresources(renderPlatform->AsD3D12CommandList(), mTextureDefault, mTextureUpload, 0, 0, 1, &textureSubData);
		subResourcesUpdated=true;
	}
#endif

	// Ensure a valid state for the resource
	if (setState)
	{
		auto curState = GetCurrentState(mip,index);
		if	((curState & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) != (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) )
		{
			SetLayout(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,mip,index);
		}
	}

	bool no_array = !cubemap && (arraySize <= 1);

	// Return array SRV / return main SRV
	if (mips <= 1 && no_array || (index < 0 && mip < 0))
	{
        if (IsCubemap() && t == crossplatform::ShaderResourceType::TEXTURE_2D_ARRAY)
        {
			return &arrayShaderResourceView12;
        }
		return &mainShaderResourceView12;
	}

	// Return main SRV / return element of array
	if (layerShaderResourceViews12 && (mip < 0 || mips <= 1))
	{
        if (index < 0 || no_array)
        {
			return &mainShaderResourceView12;
        }
		return &layerShaderResourceViews12[index];
	}

	// Return main SRV to a MIP
    if (mainMipShaderResourceViews12 && (no_array || index < 0))
    {
		return &mainMipShaderResourceViews12[mip];
    }

	// Return main SRV to a MIP of a layer
    if (layerMipShaderResourceViews12)
    {
		return &layerMipShaderResourceViews12[index][mip];
    }

	return nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12UnorderedAccessView(int index, int mip)
{
    if (mip >= mips)
    {
        mip = 0;
    }

	// Ensure a valid state for the resource
	auto curState = GetCurrentState(mip, index);
	if ((curState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		SetLayout(D3D12_RESOURCE_STATE_UNORDERED_ACCESS,mip,index);
	}

    if (mip < 0)
	{
		mip = 0;
	}
    if (index < 0)
	{
        if (mipUnorderedAccessViews12)
        {
			return &mipUnorderedAccessViews12[mip];		// UAV for the whole texture at various mips
        }
        else
        {
            index = 0;
        }
	}
    if (!layerMipUnorderedAccessViews12)
    {
		return nullptr;
    }
	return &layerMipUnorderedAccessViews12[index][mip];
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12DepthStencilView()
{
	// We need a valid render platform
	if (!renderPlatform)
	{
		SIMUL_CERR_ONCE << "This texture has not been created yet but we are trying to get a view: " << name << std::endl;
		return nullptr;
	}

	// Ensure a valid state for the resource
	auto curState = GetCurrentState();
	if ((curState & D3D12_RESOURCE_STATE_DEPTH_WRITE) != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		auto rPlat = (dx12::RenderPlatform*)renderPlatform;
		SetLayout(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	return &depthStencilView12;
}

D3D12_CPU_DESCRIPTOR_HANDLE* Texture::AsD3D12RenderTargetView(int index /*= -1*/, int mip /*= -1*/)
{
	// Ensure a valid state for the resource
	auto curState = GetCurrentState(mip, index);
	if ((curState & D3D12_RESOURCE_STATE_RENDER_TARGET) != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		SetLayout(D3D12_RESOURCE_STATE_RENDER_TARGET,mip,index);
	}
	
    if (!renderTargetViews12)
    {
		return nullptr;
    }
    if (index < 0)
    {
		index = 0;
    }
    if (mip < 0)
    {
		mip = 0;
    }
	return &renderTargetViews12[index][mip];
}

void Texture::copyToMemory(crossplatform::DeviceContext &,void *,int ,int )
{
	SIMUL_BREAK_ONCE("This is not implemented yet.");
}

void Texture::setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)
{
	HRESULT res = S_FALSE;
	
	if (mLoadedFromFile)
	{
		SIMUL_BREAK_ONCE("This is not yet supported for textures loaded from file.");
		return;
	}
	else
	{
		auto renderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
		// HACK: to make sure the cmd list is there 
		renderPlat->SetCommandList(deviceContext.asD3D12Context());
		
		if (!mTextureDefault)
		{
			SIMUL_BREAK("The texture is invalid");
			return;
		}
		renderPlat->PushToReleaseManager(mTextureUpload, "TextureSetTexelsUpload");

		// We neen to know the size of the texture (DEFAULT)
		UINT64 texSize = 0;
		renderPlat->AsD3D12Device()->GetCopyableFootprints
		(
			&mTextureDefault->GetDesc(),
			0, 1, 0,
			nullptr, nullptr, nullptr,
			&texSize
		);

		// Create the upload buffer
		res = renderPlat->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(texSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
            SIMUL_PPV_ARGS(&mTextureUpload)
		);
		SIMUL_ASSERT(res == S_OK);
		SIMUL_GPU_TRACK_MEMORY(mTextureUpload, texSize)
		mTextureUpload->SetName(L"TextureSetTexelsUpload");

		// Parameters
		int texelSize	= RenderPlatform::ByteSizeOfFormatElement(dxgi_format);
		int srcPitch	= texelSize * width;
		int srcSlice	= srcPitch * length;

		// Checks
		if (texel_index != 0)
			SIMUL_BREAK("Nacho has to implement this");
		if(srcSlice != (texelSize * (width * length)))
			SIMUL_BREAK("Nacho has to implement this");

		// Transition main texture to copy dest
		renderPlat->ResourceTransitionSimple(mTextureDefault, GetCurrentState(), D3D12_RESOURCE_STATE_COPY_DEST,true);

		// Copy the data
		D3D12_SUBRESOURCE_DATA srcData;
		srcData.pData			= src;
		srcData.RowPitch		= srcPitch;
		srcData.SlicePitch		= srcSlice;
		UpdateSubresources(renderPlat->AsD3D12CommandList(), mTextureDefault, mTextureUpload, 0, 0, 1, &srcData);

		// Transition main texture to pixel shader resource
		renderPlat->ResourceTransitionSimple(mTextureDefault, D3D12_RESOURCE_STATE_COPY_DEST, GetCurrentState(),true);
	}
}

bool Texture::IsComputable() const
{
	return (mipUnorderedAccessViews12 != nullptr || layerMipUnorderedAccessViews12 != nullptr);
}

bool Texture::HasRenderTargets() const
{
	return (renderTargetViews12 != nullptr);
}

void Texture::InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,void *srv,int w,int l,crossplatform::PixelFormat f,bool make_rt, bool setDepthStencil,bool need_srv, int numOfSamples)
{
	InitFromExternalD3D12Texture2D(renderPlatform,(ID3D12Resource*)t,(D3D12_CPU_DESCRIPTOR_HANDLE*)srv,make_rt,setDepthStencil,need_srv);
}

void Texture::SetName(const char *n)
{
	crossplatform::Texture::SetName(n);
	if(!mTextureDefault)
		return;
	std::wstring ws=simul::base::Utf8ToWString(n);
	mTextureDefault->SetName(ws.c_str());
}

void Texture::StoreExternalState(crossplatform::ResourceState resourceState)
{
	mExternalLayout=(D3D12_RESOURCE_STATES)resourceState;
	if(resourceState==crossplatform::ResourceState::UNKNOWN)
		return;
	AssumeLayout(mExternalLayout);
}

void Texture::InitFromExternalD3D12Texture2D(crossplatform::RenderPlatform* r, ID3D12Resource * t, D3D12_CPU_DESCRIPTOR_HANDLE * srv, bool make_rt, bool setDepthStencil,bool need_srv)
{
	mExternalLayout=D3D12_RESOURCE_STATE_COMMON;
	renderPlatform				= r;
	// If it's the same as before, return.
	if ((mTextureDefault == t && srv && srv->ptr == mainShaderResourceView12.ptr) && mainShaderResourceView12.ptr != -1 && (make_rt == (renderTargetViews12 != NULL)))
	{
		return;
	}
	// If it's the same texture, and we created our own srv, that's fine, return.
	if (mTextureDefault != NULL && mTextureDefault == t && mainShaderResourceView12.ptr != -1 && srv == NULL)
	{
		return;
	}

	FreeSRVTables();
	mTextureDefault				= t;
	mainShaderResourceView12	= srv? *srv : D3D12_CPU_DESCRIPTOR_HANDLE(); // What if the CPU handle changes? we should check this from outside
	mInitializedFromExternal	= true;
	// Textures initialized from external should be passed by as a SRV so we expect
	// that the resource was previously transitioned to GENERIC_READ

	if (mTextureDefault)
	{
		D3D12_RESOURCE_DESC textureDesc = mTextureDefault->GetDesc();
	
		// Assume cubemap
		if (textureDesc.DepthOrArraySize == 6)
		{
			cubemap							= true;
			textureDesc.DepthOrArraySize	= 1;
		}
		dxgi_format = textureDesc.Format;
		pixelFormat = RenderPlatform::FromDxgiFormat(dxgi_format);
		width		= (int)textureDesc.Width;
		length		= (int)textureDesc.Height;
        mNumSamples = textureDesc.SampleDesc.Count;
		if (!srv)
		{
			InitSRVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels);
			CreateSRVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels, cubemap, false, textureDesc.SampleDesc.Count > 1);
			arraySize	= textureDesc.DepthOrArraySize;
			mips		= textureDesc.MipLevels;
		}
		//InitSRVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels); //Depth Texture was not having its InitStateTable updating, thus mSubResourcesStates.size() was 0. -AJR
		depth       = textureDesc.DepthOrArraySize;

        // Create render target views for this external texture:
		if (make_rt && !setDepthStencil)
		{
            if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
            {
                FreeSRVTables();

                D3D12_RENDER_TARGET_VIEW_DESC rtDesc    = {};
                rtDesc.Format                           = RenderPlatform::TypelessToSrvFormat(textureDesc.Format);
                InitRTVTables(textureDesc.DepthOrArraySize, textureDesc.MipLevels);
                arraySize                               = textureDesc.DepthOrArraySize;
                mips                                    = textureDesc.MipLevels;
                if (renderTargetViews12)
                {
                    rtDesc.ViewDimension                    = (textureDesc.SampleDesc.Count) >1 ? D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtDesc.Texture2DArray.FirstArraySlice   = 0;
                    rtDesc.Texture2DArray.ArraySize         = 1;

                    mTextureRtHeap.Restore((dx12::RenderPlatform*)r, textureDesc.DepthOrArraySize * textureDesc.MipLevels, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "TextureRtHeap", false);

                    for (int i = 0; i < (int)textureDesc.DepthOrArraySize; i++)
                    {
                        rtDesc.Texture2DArray.FirstArraySlice = i;
                        for (int j = 0; j<(int)textureDesc.MipLevels; j++)
                        {
                            rtDesc.Texture2DArray.MipSlice = j;

                            r->AsD3D12Device()->CreateRenderTargetView(mTextureDefault, &rtDesc, mTextureRtHeap.CpuHandle());
                            renderTargetViews12[i][j] = mTextureRtHeap.CpuHandle();
                            mTextureRtHeap.Offset();
                        }
                    }
                }
            }
            else
            {
                SIMUL_CERR << "This external texture can not be created as a render target. The flags do not support it.\n";
                return;
            }
		}

        // Create depth stencil views from this texture:
        if (setDepthStencil)
        {
            if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            {
                D3D12_TEX2D_DSV dsv                     = {};
                dsv.MipSlice                            = 0;
                D3D12_DEPTH_STENCIL_VIEW_DESC depthDesc = {};
                depthDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;
                depthDesc.Format                        = RenderPlatform::TypelessToDsvFormat(textureDesc.Format); //XGI_FORMAT_D24_UNORM_S8_UINT;
                depthDesc.Flags                         = D3D12_DSV_FLAG_NONE;
                depthDesc.Texture2D                     = dsv;

                mTextureDsHeap.Restore((dx12::RenderPlatform*)renderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "Texture2DDSVHeap",false);
                renderPlatform->AsD3D12Device()->CreateDepthStencilView(mTextureDefault, &depthDesc, mTextureDsHeap.CpuHandle());
                depthStencilView12 = mTextureDsHeap.CpuHandle();
                mTextureDsHeap.Offset();

                depthStencil = true;
            }
            else
            {
                SIMUL_CERR << "This external texture can not be created as a depth stencil. The flags do not support it.\n";
            }
        }
	}
	AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
	dim			= 2;
	auto rPlat	= (dx12::RenderPlatform*)renderPlatform;
	rPlat->FlushBarriers();
}

void Texture::InitFromExternalTexture3D(crossplatform::RenderPlatform *r,void *ta,void *srv,bool make_uav)
{
}

bool Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int d,crossplatform::PixelFormat pf,bool computable,int m,bool rendertargets)
{
	HRESULT res = S_FALSE;

	pixelFormat		= pf;
	DXGI_FORMAT dxgiFormat	= dx12::RenderPlatform::ToDxgiFormat(pixelFormat);
	DXGI_FORMAT srvFormat	=dxgiFormat;
	DXGI_FORMAT uavFormat	=dxgiFormat;
	if(dxgiFormat==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		srvFormat	=dxgiFormat;
		dxgiFormat	=DXGI_FORMAT_R8G8B8A8_TYPELESS;
		uavFormat	=DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	dim				= 3;
	renderPlatform  = r;
	bool ok			= true;

	if (mTextureDefault)
	{
		auto desc = mTextureDefault->GetDesc();
		if (desc.Width != w || desc.Height != l || desc.DepthOrArraySize != d || desc.MipLevels != m || desc.Format != dxgiFormat)
			ok = false;
		if (computable != ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
			ok = false;
		if (rendertargets != ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
			ok = false;
	}
	else
	{
		ok = false;
	}
	bool changed = !ok;
	if (!ok)
	{
		dxgi_format = dxgiFormat;
		width		= w;
		length		= l;
		depth		= d;

		SIMUL_ASSERT(w > 0 && l > 0 && w <= 65536 && l <= 65536);
		InvalidateDeviceObjects();

		D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_NONE;
		textureFlags = (D3D12_RESOURCE_FLAGS)
		(
			textureFlags | 
			(computable		? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS	: 0) |
			(rendertargets	? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET		: 0)
		);

		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex3D
		(
			dxgi_format,
			width,
			length,
			depth,
			m,
			textureFlags
		);

		// Make the clear values
		D3D12_CLEAR_VALUE clearValues = {};
		if (rendertargets)
		{
			clearValues.Format		= dxgiFormat;
			clearValues.Color[0]	= 1.0f;
			clearValues.Color[1]	= 0.0f;
			clearValues.Color[2]	= 0.0f;
			clearValues.Color[3]	= 1.0f;
		}

		// Find the initial texture state
		AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
		if (rendertargets)
			AssumeLayout(D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (computable)
			AssumeLayout(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// Clean resources
		SAFE_RELEASE_LATER(mTextureDefault);


		res = r->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			GetCurrentState(),
			rendertargets? &clearValues : nullptr,
            SIMUL_PPV_ARGS(&mTextureDefault)
		);
		SIMUL_ASSERT(res == S_OK);
		size_t texSize = w * l * d * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
		SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
		std::wstring n = L"GPU_";
		n += std::wstring(name.begin(), name.end());
		mTextureDefault->SetName(n.c_str());

		// Create the main SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= dxgiFormat;
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels				= m;
		srvDesc.Texture3D.MostDetailedMip		= 0;
		FreeSRVTables();
		InitSRVTables(1, m);

		mTextureSrvHeap.Restore((dx12::RenderPlatform*)r, 1 + m, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Texture3DSrvHeap", false);
		r->AsD3D12Device()->CreateShaderResourceView(mTextureDefault,&srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();

		if(mainMipShaderResourceViews12)
		for (int j = 0; j<m; j++)
		{
			srvDesc.Texture3D.MipLevels = 1;
			srvDesc.Texture3D.MostDetailedMip = j;
			
			r->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
			mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
			mTextureSrvHeap.Offset();
		}

		if (computable && (!layerMipUnorderedAccessViews12 || !ok))
		{
			FreeUAVTables();
			InitUAVTables(1, m);
			changed = true;
			mTextureUavHeap.Restore((dx12::RenderPlatform*)r, m * 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "Texture3DUavHeap", false);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc	= {};
			uavDesc.Format								= dxgiFormat;
			uavDesc.ViewDimension						= D3D12_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D.MipSlice					= 0;
			uavDesc.Texture3D.WSize						= d;
			uavDesc.Texture3D.FirstWSlice				= 0;

			if (mipUnorderedAccessViews12)
			for (int i = 0; i < m; i++)
			{
				uavDesc.Texture3D.MipSlice = i;
				r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault,nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				mipUnorderedAccessViews12[i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
				uavDesc.Texture3D.WSize /= 2;
			}

			uavDesc.Texture3D.WSize = d;
			if (layerMipUnorderedAccessViews12)
			for (int i = 0; i<m; i++)
			{
				uavDesc.Texture3D.MipSlice = i;
				r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				layerMipUnorderedAccessViews12[0][i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
				uavDesc.Texture3D.WSize /= 2;
			}
		}
		if (d <= 8 && rendertargets && (!renderTargetViews12 || !renderTargetViews12[0] || !ok))
		{
			SIMUL_BREAK("Render targets for 3D textures are not currently supported.");
		}
	}

	mips		= m;
	arraySize	= 1;
	return changed;
}

bool Texture::EnsureTexture(crossplatform::RenderPlatform *r,crossplatform::TextureCreate *create)
{
	return EnsureTexture2DSizeAndFormat(r, create->w, create->l, create->f, create->computable, create->make_rt
		, create->setDepthStencil, create->numOfSamples, create->aa_quality, false
		, create->clear, create->clearDepth, create->clearStencil, create->compressionFormat,create->initialData);
}

bool Texture::ensureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *r,
											int w,int l,
											crossplatform::PixelFormat f,
											bool computable,bool rendertarget,bool depthstencil,
											int num_samples,int aa_quality,bool wrap,
											vec4 clear, float clearDepth, uint clearStencil)
{
	return EnsureTexture2DSizeAndFormat(r,w,l,f,computable,rendertarget,depthstencil,num_samples,aa_quality,wrap,clear,clearDepth,clearStencil);
}

bool Texture::EnsureTexture2DSizeAndFormat(	crossplatform::RenderPlatform *r,
											int w,int l,
											crossplatform::PixelFormat f,
											bool computable,bool rendertarget,bool depthstencil,
											int num_samples,int aa_quality,bool wrap,
											vec4 clear, float clearDepth, uint clearStencil
												,crossplatform::CompressionFormat cf,const void *data)
{
	// Define pixel formats of this texture
	int m			= 1;
	renderPlatform	= r;
	pixelFormat		= f;
	dxgi_format		= (DXGI_FORMAT)dx12::RenderPlatform::ToDxgiFormat(pixelFormat);
	DXGI_FORMAT texture2dFormat = dxgi_format;
	DXGI_FORMAT srvFormat		= dxgi_format;
	if (texture2dFormat == DXGI_FORMAT_D32_FLOAT)
	{
		texture2dFormat = DXGI_FORMAT_R32_TYPELESS;
		srvFormat		= DXGI_FORMAT_R32_FLOAT;
	}
	if (texture2dFormat == DXGI_FORMAT_D16_UNORM)
	{
		texture2dFormat = DXGI_FORMAT_R16_TYPELESS;
		srvFormat		= DXGI_FORMAT_R16_UNORM;
	}
	if (texture2dFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		texture2dFormat = DXGI_FORMAT_R24G8_TYPELESS;
		srvFormat		= DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	}
	dim			= 2;
	HRESULT res = S_FALSE;

	bool ok = true;
	if (mTextureDefault)
	{
		auto desc = mTextureDefault->GetDesc();
        if (desc.Width != w || desc.Height != l || desc.MipLevels != m || desc.Format != texture2dFormat)
        {
			ok = false;
        }
        if (computable != ((desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        {
			ok = false;
        }
        if (rendertarget != ((desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
        {
			ok = false;
        }
        if (depthstencil != ((desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
        {
			ok = false;
        }
        if (desc.SampleDesc.Count != num_samples)
        {
            ok = false;
        }
	}
	else
	{
		ok = false;
	}

	if (!ok)
	{
		if (w*l <= 0)
			return false;
		width = w;
		length = l;

		InvalidateDeviceObjects();
		// Check msaa support
		if (num_samples > 1)
		{
			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQuery = {};
			msaaQuery.SampleCount		= num_samples;
			msaaQuery.NumQualityLevels	= aa_quality;
			msaaQuery.Format			= srvFormat;
			res                         = renderPlatform->AsD3D12Device()->CheckFeatureSupport
            (
                D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaQuery, sizeof(msaaQuery)
            );
			if (FAILED(res))
			{
				SIMUL_CERR << "The provided quality settings are not supported by this device. \n";
				num_samples = 1;
				aa_quality	= 0;
			}
            mNumSamples = num_samples;
		}

		D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_NONE;
		textureFlags = (D3D12_RESOURCE_FLAGS)
		(
			textureFlags | 
			(computable		? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS	: 0) |
			(rendertarget	? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET		: 0) |
			(depthstencil	? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL		: 0)
		);

		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D
		(
			texture2dFormat,
			width,
			length,
			1,
			m,
			num_samples,
			aa_quality,
			textureFlags
		);

		// Make the clear values
		D3D12_CLEAR_VALUE clearValues = {};
		if (rendertarget || depthstencil)
		{
			clearValues.Format = texture2dFormat;
			if (depthstencil)
			{
				clearValues.Format					= dxgi_format;	// Clear format can't be typeless
				clearValues.DepthStencil.Depth		= clearDepth;
				clearValues.DepthStencil.Stencil	= clearStencil;
			}
			else if (!depthstencil && rendertarget)
			{
				clearValues.Color[0] = clear.x;
				clearValues.Color[1] = clear.y;
				clearValues.Color[2] = clear.z;
				clearValues.Color[3] = clear.w;
			}
		}

		// Find the initial texture state
		AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
        if (rendertarget)
        {
			AssumeLayout(D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (depthstencil)
        {
			AssumeLayout(D3D12_RESOURCE_STATE_DEPTH_READ);
        }
        if (computable)
        {
			AssumeLayout(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }

		// Clean resources
		auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;
		renderPlatformDx12->PushToReleaseManager(mTextureDefault, "mTextureDefault");
		mTextureDefault = nullptr;


		// Create the texture resource
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			GetCurrentState(),
			(rendertarget || depthstencil) ? &clearValues : nullptr,
            SIMUL_PPV_ARGS(&mTextureDefault)
		);
		SIMUL_ASSERT(res == S_OK);
		size_t texSize = w * l  * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
		SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
		std::wstring n = L"GPU_";
		n += std::wstring(name.begin(), name.end());
		mTextureDefault->SetName(n.c_str());

		// Create the main SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= srvFormat;
		srvDesc.ViewDimension					= num_samples > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels				= 1;

		mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, base::QuickFormat("Texture2DSrvHeap %s",name.c_str()), false);
		renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();

		if (computable && (!layerMipUnorderedAccessViews12 || !ok))
		{
			if (m < 1)
				m = 1;
			FreeUAVTables();
			InitUAVTables(1, m);
			mTextureUavHeap.Restore((dx12::RenderPlatform*)renderPlatform, m * 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, base::QuickFormat("Texture2DUavHeap %s",name.c_str()), false);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc	= {};
			uavDesc.Format								= texture2dFormat;
			uavDesc.ViewDimension						= D3D12_UAV_DIMENSION_TEXTURE2D;

			for (int i = 0; i<m; i++)
			{
				uavDesc.Texture2D.MipSlice = i;
				renderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault,nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				mipUnorderedAccessViews12[i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
			}

			for (int i = 0; i<m; i++)
			{
				uavDesc.Texture2D.MipSlice = i;
				renderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				layerMipUnorderedAccessViews12[0][i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
			}

		}
		if (rendertarget && (!renderTargetViews12 || !ok))
		{
			FreeRTVTables();
			InitRTVTables(1, m);
			mTextureRtHeap.Restore((dx12::RenderPlatform*)renderPlatform, 1 * m, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, base::QuickFormat("Texture2DRTHeap %s",name.c_str()), false);

			D3D12_RENDER_TARGET_VIEW_DESC rtDesc	= {};
			rtDesc.Format							= texture2dFormat;
			rtDesc.ViewDimension					= num_samples>1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;		
			for (int j = 0; j<m; j++)
			{
				rtDesc.Texture2D.MipSlice = j;
				renderPlatform->AsD3D12Device()->CreateRenderTargetView(mTextureDefault, &rtDesc,mTextureRtHeap.CpuHandle());
				renderTargetViews12[0][j] = mTextureRtHeap.CpuHandle();
				mTextureRtHeap.Offset();
			}
		}
		if (depthstencil && (!ok))
		{
			mTextureDsHeap.Restore((dx12::RenderPlatform*)renderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, base::QuickFormat("Texture2DDSVHeap %s",name.c_str()), false);
	
			D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc	= {};
			dsDesc.Format							= dxgi_format;
			dsDesc.ViewDimension					= num_samples>1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
			dsDesc.Flags							= D3D12_DSV_FLAG_NONE;
			dsDesc.Texture2D.MipSlice				= 0;

			renderPlatform->AsD3D12Device()->CreateDepthStencilView(mTextureDefault, &dsDesc, mTextureDsHeap.CpuHandle());
			depthStencilView12 = mTextureDsHeap.CpuHandle();
			mTextureDsHeap.Offset();

            depthStencil = true;
		}
		auto rPlat	= (dx12::RenderPlatform*)renderPlatform;
		rPlat->FlushBarriers();
	}

	mips		= m;
	arraySize	= 1;
	
	return !ok;
}

bool Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *r,int w,int l,int num,int m,crossplatform::PixelFormat f,bool computable,bool rendertarget,bool cubemap)
{
	bool ok			= true;
	int totalNum	= cubemap ? 6 * num : num;

	if (mTextureDefault)
	{
		auto tDesc = mTextureDefault->GetDesc();
        if (tDesc.DepthOrArraySize != totalNum || tDesc.MipLevels != m || tDesc.Width != w || tDesc.Height != l || tDesc.Format != dxgi_format)
        {
			ok = false;
        }
        if (computable != ((tDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        {
			ok = false;
        }
        if (rendertarget != ((tDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) == D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
        {
			ok = false;
        }
	}
	else
	{
		ok = false;
	}
    if (ok)
    {
		return false;
    }

	HRESULT res		= S_OK;
	renderPlatform	= r;
	pixelFormat		= f;
	dxgi_format		= dx12::RenderPlatform::ToDxgiFormat(pixelFormat);
	width			= w;
	length			= l;
	depth			= 1;
	dim				= 2;
    mips            = m;
    arraySize       = num;
    this->cubemap   = cubemap;

	D3D12_RESOURCE_FLAGS textureFlags = D3D12_RESOURCE_FLAG_NONE;
	textureFlags = (D3D12_RESOURCE_FLAGS)
	(
		textureFlags |
		(computable ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : 0) |
		(rendertarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : 0)
	);

	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		dxgi_format,
		width,
		length,
		totalNum,
		m,
		1,
		0,
		textureFlags
	);

	// Make the clear values
	D3D12_CLEAR_VALUE clearValues = {};
	if (rendertarget)
	{
		clearValues.Format = dxgi_format;
		clearValues.Color[0] = 0.0f;
		clearValues.Color[1] = 0.0f;
		clearValues.Color[2] = 0.0f;
		clearValues.Color[3] = 0.0f;
	}

	// Find the initial texture state
	AssumeLayout(D3D12_RESOURCE_STATE_GENERIC_READ);
    if (rendertarget)
    {
		AssumeLayout(D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    if (computable)
    {
		AssumeLayout(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

	// Clear resources
	SAFE_RELEASE_LATER(mTextureDefault);

	res = r->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		GetCurrentState(),
		rendertarget? &clearValues : nullptr,
        SIMUL_PPV_ARGS(&mTextureDefault)
	);
	SIMUL_ASSERT(res == S_OK);
	size_t texSize = w * l * totalNum * (dx12::RenderPlatform::ByteSizeOfFormatElement(textureDesc.Format));
	SIMUL_GPU_TRACK_MEMORY(mTextureDefault, texSize)
	std::wstring n = L"GPU_";
	n += std::wstring(name.begin(), name.end());
	mTextureDefault->SetName(n.c_str());

	FreeSRVTables();
	FreeRTVTables();
	FreeUAVTables();

	InitSRVTables(totalNum, m);
	CreateSRVTables(num, m, cubemap);

	if (computable)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

		uavDesc.Format							= dxgi_format;
		uavDesc.ViewDimension					= D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize		= totalNum;
		uavDesc.Texture2DArray.FirstArraySlice	= 0;

		InitUAVTables(totalNum, m);

		// Restore UAV heap
		uint heapSize = (mipUnorderedAccessViews12 ? m : 0) + (layerMipUnorderedAccessViews12 ? totalNum * m : 0);
		mTextureUavHeap.Restore((dx12::RenderPlatform*)r, heapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "TextureUAVHeap", false);

		if (mipUnorderedAccessViews12)
		{
			for (int i = 0; i < m; i++)
			{
				uavDesc.Texture2DArray.MipSlice = i;

				r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
				mipUnorderedAccessViews12[i] = mTextureUavHeap.CpuHandle();
				mTextureUavHeap.Offset();
			}
		}
		if (layerMipUnorderedAccessViews12)
		{
			for (int i = 0; i < totalNum; i++)
			{
				for (int j = 0; j < m; j++)
				{
					uavDesc.Texture2DArray.FirstArraySlice	= i;
					uavDesc.Texture2DArray.ArraySize		= 1;
					uavDesc.Texture2DArray.MipSlice			= j;

					r->AsD3D12Device()->CreateUnorderedAccessView(mTextureDefault, nullptr, &uavDesc, mTextureUavHeap.CpuHandle());
					layerMipUnorderedAccessViews12[i][j] = mTextureUavHeap.CpuHandle();
					mTextureUavHeap.Offset();
				}
			}
		}
	}
	if (rendertarget)
	{
		InitRTVTables(totalNum, m);

		mTextureRtHeap.Restore((dx12::RenderPlatform*)r, totalNum * m, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "TextureRTHeap", false);

		D3D12_RENDER_TARGET_VIEW_DESC rtDesc	= {};
		rtDesc.Format							= dxgi_format;
		rtDesc.ViewDimension					= D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtDesc.Texture2DArray.FirstArraySlice	= 0;
		rtDesc.Texture2DArray.ArraySize			= totalNum;
		if (renderTargetViews12)
		{
			for (int i = 0; i < totalNum; i++)
			{
				rtDesc.Texture2DArray.FirstArraySlice	= i;
				rtDesc.Texture2DArray.ArraySize			= 1;
				for (int j = 0; j < m; j++)
				{
					rtDesc.Texture2DArray.MipSlice = j;

					r->AsD3D12Device()->CreateRenderTargetView(mTextureDefault, &rtDesc, mTextureRtHeap.CpuHandle());
					renderTargetViews12[i][j] = mTextureRtHeap.CpuHandle();
					mTextureRtHeap.Offset();
				}
			}
		}
	}

	auto rPlat		= (dx12::RenderPlatform*)renderPlatform;
	rPlat->FlushBarriers();

	return true;
}

void Texture::CreateSRVTables(int num, int m, bool cubemap, bool volume, bool msaa)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	if (!volume)
	{
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format					= RenderPlatform::TypelessToSrvFormat(dxgi_format);
		srvDesc.ViewDimension			= D3D12_SRV_DIMENSION_TEXTURE2D;
		int totalNum					= cubemap ? 6 * num : num;
		if (cubemap)
		{
			if (num <= 1)
			{
				srvDesc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels		= m;
				srvDesc.TextureCube.MostDetailedMip = 0;
			}
			else
			{
				srvDesc.ViewDimension						= D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				srvDesc.TextureCubeArray.MipLevels			= m;
				srvDesc.TextureCubeArray.MostDetailedMip	= 0;
				srvDesc.TextureCubeArray.First2DArrayFace	= 0;
				srvDesc.TextureCubeArray.NumCubes			= num;
			}
		}
		else if (num > 1)
		{
			srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize		= num;
			srvDesc.Texture2DArray.FirstArraySlice	= 0;
			srvDesc.Texture2DArray.MipLevels		= m;
			srvDesc.Texture2DArray.MostDetailedMip	= 0;
		}
		else if (msaa)
		{
			srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			srvDesc.Texture2D.MipLevels			= m;
			srvDesc.Texture2D.MostDetailedMip	= 0;
		}

		// Calculate the size of the heap
		uint heapSize = 1 +
			(cubemap ? 1 : 0) +
			(mainMipShaderResourceViews12 ? m : 0) +
			(layerShaderResourceViews12 ? totalNum : 0) +
			(layerMipShaderResourceViews12 ? totalNum * m : 0);

		mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, heapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "TextureSRVHeap", false);

		// Create main srv
		renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();

		if (cubemap)
		{
			srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize		= totalNum;
			srvDesc.Texture2DArray.FirstArraySlice	= 0;
			srvDesc.Texture2DArray.MipLevels		= m;
			srvDesc.Texture2DArray.MostDetailedMip	= 0;

			// Cubemap (array) view
			renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
			arrayShaderResourceView12 = mTextureSrvHeap.CpuHandle();
			mTextureSrvHeap.Offset();
		}
		if (mainMipShaderResourceViews12)
		{
			if (cubemap)
			{
				srvDesc.ViewDimension			= D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels	= 1;
				for (int j = 0; j < m; j++)
				{
					srvDesc.TextureCube.MostDetailedMip = j;

					// View for each mip
					renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
					mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
					mTextureSrvHeap.Offset();
				}
			}
			else
			{
				for (int j = 0; j < m; j++)
				{
					// TO-DO: check this
					srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					srvDesc.Texture2DArray.ArraySize		= num;
					srvDesc.Texture2DArray.FirstArraySlice	= 0;
					srvDesc.Texture2DArray.MipLevels		= 1;
					srvDesc.Texture2DArray.MostDetailedMip	= j;

					// View for each mip
					renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
					mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
					mTextureSrvHeap.Offset();
				}
			}
		}
		if (layerShaderResourceViews12 || layerMipShaderResourceViews12)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvFaceDesc		= {};
			srvFaceDesc.Shader4ComponentMapping				= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvFaceDesc.Format								= srvDesc.Format;
			srvFaceDesc.ViewDimension						= D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			for (int i = 0; i < totalNum; i++)
			{
				if (layerShaderResourceViews12)
				{
					srvFaceDesc.Texture2DArray.ArraySize		= 1;
					srvFaceDesc.Texture2DArray.FirstArraySlice	= i;
					srvFaceDesc.Texture2DArray.MipLevels		= m;
					srvFaceDesc.Texture2DArray.MostDetailedMip	= 0;

					renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvFaceDesc, mTextureSrvHeap.CpuHandle());
					layerShaderResourceViews12[i] = mTextureSrvHeap.CpuHandle();
					mTextureSrvHeap.Offset();
				}
				if (layerMipShaderResourceViews12)
				{
					srvFaceDesc.Texture2DArray.ArraySize		= 1;
					srvFaceDesc.Texture2DArray.FirstArraySlice	= i;
					srvFaceDesc.Texture2DArray.MipLevels		= 1;

					for (int j = 0; j < m; j++)
					{
						srvFaceDesc.Texture2DArray.MostDetailedMip	= j;

						renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvFaceDesc, mTextureSrvHeap.CpuHandle());
						layerMipShaderResourceViews12[i][j] = mTextureSrvHeap.CpuHandle();
						mTextureSrvHeap.Offset();
					}
				}
			}
		}
	}
	else
	{
		// Calculate the size of the heap
		uint heapSize = 1 +
			(mainMipShaderResourceViews12 ? m : 0);

		mTextureSrvHeap.Restore((dx12::RenderPlatform*)renderPlatform, heapSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "TextureSRVHeap", false);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc		= {};
		srvDesc.Shader4ComponentMapping				= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format								= RenderPlatform::TypelessToSrvFormat(dxgi_format);
		srvDesc.ViewDimension						= D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels					= m;
		srvDesc.Texture3D.MostDetailedMip			= 0;

		// Main shader view
		renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
		mainShaderResourceView12 = mTextureSrvHeap.CpuHandle();
		mTextureSrvHeap.Offset();

		if (mainMipShaderResourceViews12)
		{
			for (int j = 0; j < m; j++)
			{
				srvDesc.Texture3D.MipLevels			= 1;
				srvDesc.Texture3D.MostDetailedMip	= j;

				// View of the mips
				renderPlatform->AsD3D12Device()->CreateShaderResourceView(mTextureDefault, &srvDesc, mTextureSrvHeap.CpuHandle());
				mainMipShaderResourceViews12[j] = mTextureSrvHeap.CpuHandle();
				mTextureSrvHeap.Offset();
			}
		}
	}
}

void Texture::ensureTexture1DSizeAndFormat(ID3D12Device* pd3dDevice,int w,crossplatform::PixelFormat pf,bool computable)
{
}

void Texture::ClearDepthStencil(crossplatform::DeviceContext& deviceContext, float depthClear, int stencilClear)
{
    const D3D12_CLEAR_FLAGS kFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
    deviceContext.asD3D12Context()->ClearDepthStencilView(*AsD3D12DepthStencilView(), kFlags, depthClear, stencilClear, 0, nullptr);
}

void Texture::GenerateMips(crossplatform::DeviceContext& deviceContext)
{
    if (mips == 1)
    {
        SIMUL_CERR_ONCE << "Calling GenerateMips on the texture: " << name << " which only has 1 mip (this has no effect). \n";
        return;
    }
    deviceContext.renderPlatform->GenerateMips(deviceContext, this, true, 0);
}


bool Texture::isMapped() const
{
	return false;
}

void Texture::unmap()
{
}

vec4 Texture::GetTexel(crossplatform::DeviceContext &,vec2 ,bool )
{
    SIMUL_BREAK("")
	return vec4(0.0f,0.0f,0.0f,0.0f);
}

void Texture::activateRenderTarget(crossplatform::DeviceContext &deviceContext,int array_index,int mip)
{
    if (array_index < 0)
    {
		array_index = 0;
    }
    if (mip < 0)
    {
		mip = 0;
    }
    if (mip >= mips)
    {
		return;
    }
	
	if (renderTargetViews12)
	{
		auto rp		= (dx12::RenderPlatform*)deviceContext.renderPlatform;
		auto rtView = AsD3D12RenderTargetView(array_index, mip);

        targetsAndViewport.num				= 1;
        targetsAndViewport.m_dt				= nullptr;
        targetsAndViewport.m_rt[0]			= rtView;
        targetsAndViewport.rtFormats[0]     = pixelFormat;
        targetsAndViewport.viewport.w		= (int)(std::max(1, (width >> mip)));
        targetsAndViewport.viewport.h		= (int)(std::max(1, (length >> mip)));
        targetsAndViewport.viewport.x		= 0;
        targetsAndViewport.viewport.y		= 0;

        rp->ActivateRenderTargets(deviceContext, &targetsAndViewport);
        
        // Inform the render platform of the current number of MSAA samples
        mCachedMSAAState = rp->GetMSAAInfo();
        rp->SetCurrentSamples(mNumSamples);
	}
	SetLayout(D3D12_RESOURCE_STATE_RENDER_TARGET,mip,array_index);
}

void Texture::deactivateRenderTarget(crossplatform::DeviceContext &deviceContext)
{
	auto rp = (dx12::RenderPlatform*)deviceContext.renderPlatform;
    rp->DeactivateRenderTargets(deviceContext);
    // Restore MSAA state
    rp->SetCurrentSamples(mCachedMSAAState.Count,mCachedMSAAState.Quality);
}

int Texture::GetSampleCount()const
{
    return mNumSamples == 1 ? 0 : mNumSamples;
}

D3D12_RESOURCE_STATES Texture::GetCurrentState(int mip /*= -1*/, int index /*= -1*/)
{
	// Return the resource state
	if (mip == -1 && index == -1)
	{
        size_t numLayers = mSubResourcesStates.size();
		// If we request the state of the whole resource, we have to make sure
		// that all of the subresources are in the correct state. The correct state
		// will be the main resource state.
        if (!mSubResourcesStates.empty())
        {
			auto rPlat = (dx12::RenderPlatform*)renderPlatform;
			for (unsigned int l = 0; l < numLayers; l++)
			{
				for ( int m = 0; m < mips; m++)
				{
					auto curState = mSubResourcesStates[l][m];
					if (curState != mResourceState)
					{
						rPlat->ResourceTransitionSimple(mTextureDefault, curState, mResourceState, true,
														RenderPlatform::GetResourceIndex(m,l,mips, numLayers));
						mSubResourcesStates[l][m] = mResourceState;
					}
				}
			}
		}
		split_layouts=false;
		return mResourceState;
	}
	// Return a subresource state
	int curMip		= (mip == -1) ? 0 : mip;
	int curLayer	= (index == -1)? 0 : index;
	if (mSubResourcesStates.empty()) //Temporary fixed - AJR
	{
		//SIMUL_CERR << "mSubResourcesStates.empty() = true. Returning mResourceState." << std::endl;
		return mResourceState;
	}
	return mSubResourcesStates[curLayer][curMip];
}

void Texture::SplitLayouts()
{
	split_layouts=true;
}

void Texture::AssumeLayout(D3D12_RESOURCE_STATES state)
{
    int numLayers       = mSubResourcesStates.size();
	mResourceState      = state;
	// And set all the subresources to that state
	// We understand that we transitioned ALL the resources
    if (!mSubResourcesStates.empty())
    {
        for (int l = 0; l < numLayers; l++)
	    {
	    	for (int m = 0; m < mips; m++)
	    	{
	    		mSubResourcesStates[l][m] = state;
	    	}
	    }
    }
	split_layouts=false;
}

void Texture::SetLayout(D3D12_RESOURCE_STATES state, int mip /*= -1*/, int index /*= -1*/)
{
	auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	int curArray = arraySize;
	if (cubemap)
	{
		curArray *= 6;
	}
	// Set the resource state
	if (mip == -1 && index == -1)
	{
		int numLayers       = mSubResourcesStates.size();
		if (split_layouts)
		{
		// And set all the subresources to that state
		// We understand that we transitioned ALL the resources
			if (!mSubResourcesStates.empty())
			{
				for (int l = 0; l < numLayers; l++)
				{
		    		for (int m = 0; m < mips; m++)
		    		{
						if(mSubResourcesStates[l][m]!=state)
							rPlat->ResourceTransitionSimple(mTextureDefault, mSubResourcesStates[l][m], state,true, RenderPlatform::GetResourceIndex(mip, index, mips, curArray));
		    			mSubResourcesStates[l][m] = state;
		    		}
				}
			}
			//rPlat->ResourceTransitionSimple(mTextureDefault, mResourceState, state, true, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
		else if(mResourceState!=state)
		{
			rPlat->ResourceTransitionSimple(mTextureDefault, mResourceState, state, true, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			for (int l = 0; l < numLayers; l++)
			{
		    	for (int m = 0; m < mips; m++)
		    	{
		    		mSubResourcesStates[l][m] = state;
		    	}
			}
		}
		mResourceState      = state;
		split_layouts=false;
	}
	// Set a subresource state
	else
	{
		int curMip		= (mip == -1) ? 0 : mip;
		int curLayer	= (index == -1) ? 0 : index;
		
		if (mSubResourcesStates.empty()) //Temporary fixed - AJR
		{
			//SIMUL_CERR << "mSubResourcesStates.empty() = true. Setting state into mSubResourcesStates[0][0]." << std::endl;
			mSubResourcesStates.push_back({ state });
		}
		if(state!=mSubResourcesStates[curLayer][curMip])
			rPlat->ResourceTransitionSimple(mTextureDefault, mSubResourcesStates[curLayer][curMip], state, true, RenderPlatform::GetResourceIndex(mip, index, mips, curArray));

		mSubResourcesStates[curLayer][curMip] = state;
		// Array Size == 1 && mips = 1 (it only has 1 sub resource)
		// the asSRV code will return the mainSRV!
		bool no_array = !cubemap && (arraySize <= 1);
		if (mip==-1 && curLayer==-1)
		{
			mResourceState = state;
		}
		else if(mResourceState!=state)
			split_layouts=true;
	}
}

void Texture::RestoreExternalTextureState(crossplatform::DeviceContext &deviceContext)
{
	if((crossplatform::ResourceState)mExternalLayout==crossplatform::ResourceState::UNKNOWN)
		return;
	auto rPlat		= (dx12::RenderPlatform*)(renderPlatform);
	SetLayout( mExternalLayout);
}

void Texture::InitSRVTables(int l,int m)
{
	InitStateTable(l, m);

	// SRV's for each layer, including all mips
	if (l>1)
		layerShaderResourceViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE[l];				
	else
		layerShaderResourceViews12 = nullptr;

	// SRV's for the whole texture at different mips.
	if (m>1)
		mainMipShaderResourceViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE[m];				
	else
		mainMipShaderResourceViews12 = nullptr;

	// SRV's for each layer at different mips.
	if (l>1 && m>1)
	{
		layerMipShaderResourceViews12 = new D3D12_CPU_DESCRIPTOR_HANDLE*[l];		
		// SRV's for each layer at different mips.
		for (int i = 0; i<l; i++)
			layerMipShaderResourceViews12[i] = new D3D12_CPU_DESCRIPTOR_HANDLE[m];		
	}
	else
		layerMipShaderResourceViews12 = nullptr;
}

void Texture::FreeSRVTables()
{
	// Check  that cubemap and array size have already being set before we call this method
	int total_num = cubemap ? arraySize * 6 : arraySize;

	if (layerShaderResourceViews12)
		delete[] layerShaderResourceViews12;
	layerShaderResourceViews12 = nullptr;

	for (int i = 0; i<total_num; i++)
	{		
		if (layerMipShaderResourceViews12)
		{
			delete[] layerMipShaderResourceViews12[i];
		}
	}
	delete[] layerShaderResourceViews12;
	layerShaderResourceViews12 = nullptr;
	delete[] layerMipShaderResourceViews12;
	layerMipShaderResourceViews12 = nullptr;

	if (mainMipShaderResourceViews12)
	{
		delete[] mainMipShaderResourceViews12;
	}
	mainMipShaderResourceViews12 = nullptr;
}