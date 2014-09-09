#define NOMINMAX
#include "Utilities.h"
#include "MacrosDX1x.h"
#include "Simul\Base\StringToWString.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Math/Vector3.h"
#if WINVER<0x602
#include <d3dx11.h>
#endif
#include <algorithm>			// for std::min / max
#include "D3dx11effect.h"
using namespace simul;
using namespace dx11;


ComputableTexture::ComputableTexture()
	:g_pTex_Output(NULL)
	,g_pUAV_Output(NULL)
	,g_pSRV_Output(NULL)
{
}

ComputableTexture::~ComputableTexture()
{
}

void ComputableTexture::release()
{
	SAFE_RELEASE(g_pTex_Output);
	SAFE_RELEASE(g_pUAV_Output);
	SAFE_RELEASE(g_pSRV_Output);
}

void ComputableTexture::init(ID3D11Device *pd3dDevice,int w,int h)
{
	release();
    D3D11_TEXTURE2D_DESC tex_desc;
	ZeroMemory(&tex_desc, sizeof(D3D11_TEXTURE2D_DESC));
	tex_desc.ArraySize			= 1;
    tex_desc.BindFlags			= D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    tex_desc.Usage				= D3D11_USAGE_DEFAULT;
    tex_desc.Width				= w;
    tex_desc.Height				= h;
    tex_desc.MipLevels			= 1;
    tex_desc.SampleDesc.Count	= 1;
	tex_desc.Format				= DXGI_FORMAT_R32_UINT;
    pd3dDevice->CreateTexture2D(&tex_desc, NULL, &g_pTex_Output);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	uav_desc.Format				= tex_desc.Format;
	uav_desc.ViewDimension		= D3D11_UAV_DIMENSION_TEXTURE2D;
	uav_desc.Texture2D.MipSlice	= 0;
	pd3dDevice->CreateUnorderedAccessView(g_pTex_Output, &uav_desc, &g_pUAV_Output);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    srv_desc.Format						= tex_desc.Format;
    srv_desc.ViewDimension				= D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels		= 1;
    srv_desc.Texture2D.MostDetailedMip	= 0;
    pd3dDevice->CreateShaderResourceView(g_pTex_Output, &srv_desc, &g_pSRV_Output);
}

/*
create the associated Texture2D resource as a Texture2D array resource, and then create a shader resource view for that resource.

1. Load all of the textures using D3DX10CreateTextureFromFile, with a D3DX10_IMAGE_LOAD_INFO specifying that you want D3D10_USAGE_STAGING.
		all of your textures need to be the same size, have the same format, and have the same number of mip levels
2. Map every mip level of every texture
3. Set up an array of D3D10_SUBRESOURCE_DATA's with number of elements == number of textures * number of mip levels. 
4. For each texture, set the pSysMem member of a D3D10_SUBRESOURCE_DATA to the pointer you got when you mapped each mip level of each texture. Make sure you also set the SysMemPitch to the pitch you got when you mapped that mip level
5. Call CreateTexture2D with the desired array size, and pass the array of D3D10_SUBRESOURCE_DATA's
6. Create a shader resource view for the texture 
*/
void ArrayTexture::create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files)
{
	release();
	std::vector<ID3D11Texture2D *> textures;
	for(unsigned i=0;i<texture_files.size();i++)
	{
		textures.push_back(simul::dx11::LoadStagingTexture(pd3dDevice,texture_files[i].c_str()));
	}
	D3D11_TEXTURE2D_DESC desc;
//	D3D11_SUBRESOURCE_DATA *subResources=new D3D11_SUBRESOURCE_DATA[textures.size()];
	ID3D11DeviceContext *pContext=NULL;
	pd3dDevice->GetImmediateContext(&pContext);
	for(int i=0;i<(int)textures.size();i++)
	{
		if(!textures[i])
			return;
		textures[i]->GetDesc(&desc);
	/*	D3D11_MAPPED_SUBRESOURCE mapped_res;
		HRESULT hr=pContext->Map(textures[i],0,D3D11_MAP_READ,0,&mapped_res);	
		if(hr==S_OK)
		{
		subResources[i].pSysMem			=mapped_res.pData;
		subResources[i].SysMemPitch		=mapped_res.RowPitch;
		subResources[i].SysMemSlicePitch=mapped_res.DepthPitch;
		}*/
	}
	static int num_mips=5;
	desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
	desc.Usage=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags=0;
	desc.ArraySize=(UINT)textures.size();
	desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels=num_mips;
	pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture);

	if(m_pArrayTexture)
	for(unsigned i=0;i<textures.size();i++)
	{
		// Copy the resource directly, no CPU mapping
		pContext->CopySubresourceRegion(
						m_pArrayTexture
						,i*num_mips
						,0
						,0
						,0
						,textures[i]
						,0
						,NULL
						);
		//pContext->UpdateSubresource(m_pArrayTexture,i*num_mips, NULL,subResources[i].pSysMem,subResources[i].SysMemPitch,subResources[i].SysMemSlicePitch);
	}
	pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV);
	//delete [] subResources;
	for(unsigned i=0;i<textures.size();i++)
	{
	//	pContext->Unmap(textures[i],0);
		SAFE_RELEASE(textures[i])
	}
	pContext->GenerateMips(m_pArrayTexture_SRV);
	SAFE_RELEASE(pContext)
}

void ArrayTexture::create(ID3D11Device *pd3dDevice,int w,int l,int num,DXGI_FORMAT f,bool computable)
{
	release();
	D3D11_TEXTURE2D_DESC desc;
//	D3D11_SUBRESOURCE_DATA *subResources=new D3D11_SUBRESOURCE_DATA[num];
	//ID3D11DeviceContext *pContext=NULL;
	//pd3dDevice->GetImmediateContext(&pContext);
	static int num_mips		=5;
	desc.Width				=w;
	desc.Height				=l;
	desc.Format				=f;
	desc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_RENDER_TARGET ;
	desc.Usage				=D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags		=0;
	desc.ArraySize			=num;
	desc.MiscFlags			=D3D11_RESOURCE_MISC_GENERATE_MIPS;
	desc.MipLevels			=num_mips;
	desc.SampleDesc.Count	=1;
	desc.SampleDesc.Quality	=0;
	V_CHECK(pd3dDevice->CreateTexture2D(&desc,NULL,&m_pArrayTexture));
	V_CHECK(pd3dDevice->CreateShaderResourceView(m_pArrayTexture,NULL,&m_pArrayTexture_SRV));
	V_CHECK(pd3dDevice->CreateUnorderedAccessView(m_pArrayTexture,NULL,&unorderedAccessView));
	//SAFE_RELEASE(pContext)
}

int UtilityRenderer::instance_count=0;
int UtilityRenderer::screen_width=0;
int UtilityRenderer::screen_height=0;
UtilityRenderer utilityRenderer;

UtilityRenderer::UtilityRenderer()
{
	instance_count++;
}

UtilityRenderer::~UtilityRenderer()
{
	// Now calling this manually instead to avoid global destruction when memory has already been freed.
	//InvalidateDeviceObjects();
}

struct Vertext
{
	D3DXVECTOR4 pos;
	D3DXVECTOR2 tex;
};

void UtilityRenderer::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void UtilityRenderer::GetScreenSize(int& w,int& h)
{
	w=screen_width;
	h=screen_height;
}


void UtilityRenderer::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(NULL);
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}			

void UtilityRenderer::DrawQuad2(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,ID3DX11Effect* eff,ID3DX11EffectTechnique* tech,int pass)
{
	DrawQuad2(deviceContext
		,2.f*(float)x1/(float)screen_width-1.f
		,1.f-2.f*(float)(y1+dy)/(float)screen_height
		,2.f*(float)dx/(float)screen_width
		,2.f*(float)dy/(float)screen_height
		,eff,tech,pass);
}

void UtilityRenderer::DrawQuad2(crossplatform::DeviceContext &deviceContext,float x1,float y1,float dx,float dy,ID3DX11Effect* eff
								,ID3DX11EffectTechnique* tech
								,int pass)
{
	HRESULT hr=S_OK;
	setParameter(eff,"rect",x1,y1,dx,dy);
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(pContext,tech->GetPassByIndex(pass));
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
	unbindTextures(eff);
	ApplyPass(pContext,tech->GetPassByIndex(pass));
}

void UtilityRenderer::RenderAngledQuad(crossplatform::DeviceContext &deviceContext
									   ,const float *dr
									   ,float half_angle_radians
										,ID3DX11Effect* effect
										,ID3DX11EffectTechnique* tech
										,D3DXMATRIX view
										,D3DXMATRIX proj
										,D3DXVECTOR3 sun_dir)
{
	// If y is vertical, we have LEFT-HANDED rotations, otherwise right.
	// But D3DXMatrixRotationYawPitchRoll uses only left-handed, hence the change of sign below.
	if(effect)
	{
//		setMatrix(effect,"worldViewProj",tmp1);
		//setParameter(effect,"lightDir",sun2);
	//	setParameter(effect,"radiusRadians",half_angle_radians);
	}
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void UtilityRenderer::DrawSphere(crossplatform::DeviceContext &deviceContext,int latitudes,int longitudes)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	// The number of indices per lat strip is (longitudes+1)*2.
	// The number of lat strips is (latitudes+1)
	pContext->Draw((longitudes+1)*(latitudes+1)*2,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}