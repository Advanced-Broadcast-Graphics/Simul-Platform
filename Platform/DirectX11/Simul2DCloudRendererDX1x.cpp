// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRendererDX11.cpp A renderer for 2D cloud layers.
#define NOMINMAX
#include "Simul2DCloudRendererdx1x.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "Simul/LicenseKey.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/DirectX11/Profiler.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

Simul2DCloudRendererDX11::Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem) :
	simul::clouds::Base2DCloudRenderer(ck,mem)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,msaaTechnique(NULL)
	,technique(NULL)
	,vertexBuffer(NULL)
	,indexBuffer(NULL)
	,inputLayout(NULL)
{
}

Simul2DCloudRendererDX11::~Simul2DCloudRendererDX11()
{
	InvalidateDeviceObjects();
}

void Simul2DCloudRendererDX11::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseCloudRenderer::RestoreDeviceObjects(r);
	m_pd3dDevice=renderPlatform->AsD3D11Device();
    RecompileShaders();
	SAFE_RELEASE(inputLayout);
	if(technique)
	{
		D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT	,0,0	,D3D11_INPUT_PER_VERTEX_DATA,0},
		};
		D3D1x_PASS_DESC PassDesc;
		technique->GetPassByIndex(0)->GetDesc(&PassDesc);
		m_pd3dDevice->CreateInputLayout(decl,1, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &inputLayout);
	}
	static float max_cloud_distance=1.f;
	helper->SetGrid(4,4);
	helper->MakeDefaultGeometry(max_cloud_distance);
	const simul::clouds::Cloud2DGeometryHelper::VertexVector &vertices=helper->GetVertices();
	simul::clouds::Cloud2DGeometryHelper::Vertex *v=::new(memoryInterface) simul::clouds::Cloud2DGeometryHelper::Vertex[vertices.size()];
	for(int i=0;i<(int)vertices.size();i++)
		v[i]=vertices[i];
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData,sizeof(D3D1x_SUBRESOURCE_DATA));
    InitData.pSysMem	=v;
    InitData.SysMemPitch=sizeof(simul::clouds::Cloud2DGeometryHelper::Vertex);
	D3D11_BUFFER_DESC desc=
	{
		(UINT)(vertices.size()*sizeof(simul::clouds::Cloud2DGeometryHelper::Vertex)),
        D3D11_USAGE_IMMUTABLE,
        D3D11_BIND_VERTEX_BUFFER,
        0,0
	};
	SAFE_RELEASE(vertexBuffer);
	m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);
	::operator delete [](v,memoryInterface);
	const simul::clouds::Cloud2DGeometryHelper::QuadStripVector &quads=helper->GetQuadStrips();
	num_indices=0;
	for(int i=0;i<(int)quads.size();i++)
		num_indices+=(int)quads[i].indices.size()+2;
	num_indices+=((int)quads.size()-1)*2;
	unsigned short *indices=new unsigned short[num_indices];
	int n=0;
	for(int i=0;i<(int)quads.size();i++)
	{
		if(i)
			for(int j=0;j<2;j++)
				indices[n++]=quads[i].indices[j];
		for(int j=0;j<(int)quads[i].indices.size();j++)
			indices[n++]=quads[i].indices[j];
	}
	// index buffer
	D3D11_BUFFER_DESC indexBufferDesc=
	{
        num_indices*sizeof(unsigned short),
        D3D11_USAGE_IMMUTABLE,
        D3D11_BIND_INDEX_BUFFER,
        0,
        0
	};
    ZeroMemory(&InitData,sizeof(D3D11_SUBRESOURCE_DATA));
    InitData.pSysMem		=indices;
    InitData.SysMemPitch	=sizeof(unsigned short);
	SAFE_RELEASE(indexBuffer);
	m_pd3dDevice->CreateBuffer(&indexBufferDesc,&InitData,&indexBuffer);
	delete [] indices;

	cloud2DConstants.RestoreDeviceObjects(m_pd3dDevice);
	detail2DConstants.RestoreDeviceObjects(m_pd3dDevice);

	coverage_fb.RestoreDeviceObjects(renderPlatform);

	detail_fb.SetWidthAndHeight(256,256);
	detail_fb.RestoreDeviceObjects(renderPlatform);
	detail_fb.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
	
	noise_fb.RestoreDeviceObjects(renderPlatform);
	noise_fb.SetWidthAndHeight(16,16);
	noise_fb.SetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);

	dens_fb.RestoreDeviceObjects(renderPlatform);
	dens_fb.SetWidthAndHeight(512,512);
	dens_fb.SetFormat(DXGI_FORMAT_R16_FLOAT);
}

void Simul2DCloudRendererDX11::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]=ReverseDepth?"1":"0";
	defines["USE_LIGHT_TABLES"]=UseLightTables?"1":"0";
	CreateEffect(m_pd3dDevice,&effect,"simul_clouds_2d.fx",defines);
	msaaTechnique=effect->GetTechniqueByName("simul_clouds_2d_msaa");
	technique	=effect->GetTechniqueByName("simul_clouds_2d");
	cloud2DConstants.LinkToEffect(effect,"Cloud2DConstants");
	detail2DConstants.LinkToEffect(effect,"Detail2DConstants");
}

void Simul2DCloudRendererDX11::RenderDetailTexture(crossplatform::DeviceContext &deviceContext)
{
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
    ProfileBlock profileBlock(pContext,"Simul2DCloudRendererDX11::RenderDetailTexture");

	int noise_texture_size		=cloudKeyframer->GetEdgeNoiseTextureSize();
	int noise_texture_frequency	=cloudKeyframer->GetEdgeNoiseFrequency();
	
	noise_fb.SetWidthAndHeight(noise_texture_frequency,noise_texture_frequency);
	noise_fb.SetFormat(DXGI_FORMAT_R32G32B32A32_FLOAT);
	noise_fb.Activate(deviceContext);
	{
		SetDetail2DCloudConstants(detail2DConstants);
		detail2DConstants.Apply(deviceContext);
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("random");
		t->GetPassByIndex(0)->Apply(0,pContext);
		renderPlatform->DrawQuad(deviceContext);
	} 
	noise_fb.Deactivate(pContext);
	dens_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
	dens_fb.Activate(deviceContext);
	{
		SetDetail2DCloudConstants(detail2DConstants);
		detail2DConstants.Apply(deviceContext);
		simul::dx11::setTexture(effect,"imageTexture"	,(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("detail_density");
		t->GetPassByIndex(0)->Apply(0,pContext);
		renderPlatform->DrawQuad(deviceContext);
	}
	dens_fb.Deactivate(pContext);
	detail_fb.SetWidthAndHeight(noise_texture_size,noise_texture_size);
	detail_fb.Activate(deviceContext);
	{
		simul::dx11::setTexture(effect,"imageTexture",(ID3D11ShaderResourceView*)dens_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("detail_lighting");
		t->GetPassByIndex(0)->Apply(0,pContext);
		renderPlatform->DrawQuad(deviceContext);
	}
	detail_fb.Deactivate(pContext);
	coverage_fb.Activate(deviceContext);
	{
		simul::dx11::setTexture(effect,"noiseTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
		ID3DX11EffectTechnique *t=effect->GetTechniqueByName("coverage");
		t->GetPassByIndex(0)->Apply(0,pContext);
		renderPlatform->DrawQuad(deviceContext);
	}
	coverage_fb.Deactivate(pContext);
}

void Simul2DCloudRendererDX11::InvalidateDeviceObjects()
{
	SAFE_RELEASE(effect);
	SAFE_RELEASE(vertexBuffer);
	m_pd3dDevice=NULL;
	SAFE_RELEASE(inputLayout);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	cloud2DConstants.InvalidateDeviceObjects();
	detail2DConstants.InvalidateDeviceObjects();
	detail_fb.InvalidateDeviceObjects();
	noise_fb.InvalidateDeviceObjects();
	dens_fb.InvalidateDeviceObjects();
	
	coverage_fb.InvalidateDeviceObjects();
}

void Simul2DCloudRendererDX11::EnsureCorrectTextureSizes()
{
	simul::sky::int3 i=cloudKeyframer->GetTextureSizes();
	int width_x=i.x;
	int length_y=i.y;
	coverage_fb.SetWidthAndHeight(width_x,length_y);
	if(cloud_tex_width_x==width_x&&cloud_tex_length_y==length_y&&cloud_tex_depth_z==1)
		return;
	cloud_tex_width_x=width_x;
	cloud_tex_length_y=length_y;
	cloud_tex_depth_z=1;
	
	
}

void Simul2DCloudRendererDX11::EnsureTexturesAreUpToDate(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
    SIMUL_COMBINED_PROFILE_START(pContext,"Simul2DCloudRendererDX11::EnsureTexturesAreUpToDate");
	EnsureCorrectTextureSizes();
	EnsureTextureCycle();
    SIMUL_COMBINED_PROFILE_END(pContext)
}

void Simul2DCloudRendererDX11::EnsureTextureCycle()
{
	int cyc=(cloudKeyframer->GetTextureCycle())%3;
	while(texture_cycle!=cyc)
	{
		std::swap(seq_texture_iterator[0],seq_texture_iterator[1]);
		std::swap(seq_texture_iterator[1],seq_texture_iterator[2]);
		texture_cycle++;
		texture_cycle=texture_cycle%3;
		if(texture_cycle<0)
			texture_cycle+=3;
	}
}

void Simul2DCloudRendererDX11::PreRenderUpdate(crossplatform::DeviceContext &deviceContext)
{
	EnsureTexturesAreUpToDate(deviceContext);
	RenderDetailTexture(deviceContext);
}


bool Simul2DCloudRendererDX11::Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap,crossplatform::NearFarPass nearFarPass
									  ,crossplatform::Texture *depthTexture,bool write_alpha
									  ,const simul::sky::float4& viewportTextureRegionXYWH,const simul::sky::float4& )
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.platform_context;
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"2DCloudRenderer")
	
	ID3D11ShaderResourceView* depthTexture_SRV	=depthTexture->AsD3D11ShaderResourceView();
	ID3DX11EffectTechnique*		tech			=technique;
	if(depthTexture_SRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc;
		depthTexture_SRV->GetDesc(&depthDesc);
		if(depthTexture&&depthDesc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS)
			tech=msaaTechnique;
	}
	if(!tech)
		return false;

	simul::dx11::setTexture(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());
	simul::dx11::setTexture(effect,"noiseTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
	simul::dx11::setTexture(effect,"coverageTexture",(ID3D11ShaderResourceView*)coverage_fb.GetColorTex());
	simul::dx11::setTexture(effect,"lossTexture",skyLossTexture->AsD3D11ShaderResourceView());
	simul::dx11::setTexture(effect,"inscTexture",overcInscTexture->AsD3D11ShaderResourceView());
	simul::dx11::setTexture(effect,"skylTexture",skylightTexture->AsD3D11ShaderResourceView());
	// Set both MS and regular - we'll only use one of them:
	if(depthTexture->GetSampleCount()>0)
		simul::dx11::setTexture(effect,"depthTextureMS",depthTexture_SRV);
	else
		simul::dx11::setTexture(effect,"depthTexture",depthTexture_SRV);
	simul::dx11::setTexture(effect,"illuminationTexture",illuminationTexture->AsD3D11ShaderResourceView());
	simul::dx11::setTexture(effect,"lightTableTexture",lightTableTexture->AsD3D11ShaderResourceView());
	

	math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(deviceContext.viewStruct.view,false);
	float ir_integration_factors[]={0,0,0,0};

	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Set constants")
	Set2DCloudConstants(cloud2DConstants,deviceContext.viewStruct.view,deviceContext.viewStruct.proj,exposure,viewportTextureRegionXYWH,ir_integration_factors);
	cloud2DConstants.Apply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)

	ID3D11InputLayout* previousInputLayout;
	UINT prevOffset;
	DXGI_FORMAT prevFormat;
	ID3D11Buffer* pPrevBuffer;
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;

	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IAGetInputLayout(&previousInputLayout);
	pContext->IAGetIndexBuffer(&pPrevBuffer, &prevFormat, &prevOffset);

	pContext->IASetInputLayout(inputLayout);
	SET_VERTEX_BUFFER(pContext,vertexBuffer,simul::clouds::Cloud2DGeometryHelper::Vertex);
	pContext->IASetIndexBuffer(indexBuffer,DXGI_FORMAT_R16_UINT,0);					

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ApplyPass(pContext,tech->GetPassByIndex(0));
	
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"DrawIndexed")

	pContext->DrawIndexed(num_indices-2,0,0);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout(previousInputLayout);
	pContext->IASetIndexBuffer(pPrevBuffer, prevFormat, prevOffset);

	SAFE_RELEASE(previousInputLayout)
	SAFE_RELEASE(pPrevBuffer);

	simul::dx11::setTexture(effect,"imageTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"noiseTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"coverageTexture"		,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"lossTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"inscTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"skylTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"depthTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"depthTextureMS"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture(effect,"illuminationTexture"	,(ID3D11ShaderResourceView*)NULL);
	ApplyPass(pContext,tech->GetPassByIndex(0));
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	return true;
}

void Simul2DCloudRendererDX11::RenderCrossSections(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	static int u=8;
	int w=(width-8)/u;
	if(w>height/2)
		w=height/2;
	simul::clouds::CloudGridInterface *gi=GetCloudGridInterface();
	int h=w/gi->GetGridWidth();
	if(h<1)
		h=1;
	h*=gi->GetGridHeight();
	static float mult=1.f;
	simul::dx11::UtilityRenderer::SetScreenSize(width,height);
	for(int i=0;i<3;i++)
	{
		const simul::clouds::CloudKeyframer::Keyframe *kf=
				static_cast<simul::clouds::CloudKeyframer::Keyframe *>(cloudKeyframer->GetKeyframe(
				cloudKeyframer->GetKeyframeAtTime(skyInterface->GetTime())+i));
		if(!kf)
			break;
		simul::sky::float4 light_response(mult*kf->direct_light,mult*kf->indirect_light,mult*kf->ambient_light,0);
	}
	simul::dx11::setTexture(effect,"imageTexture",(ID3D11ShaderResourceView*)coverage_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(deviceContext,(0)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	renderPlatform->Print(deviceContext,(0)*(w+8)+8,height-8-w,"coverage");
	simul::dx11::setTexture(effect,"imageTexture",(ID3D11ShaderResourceView*)noise_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(deviceContext,(1)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	renderPlatform->Print(deviceContext,(1)*(w+8)+8,height-8-w,"noise");
	simul::dx11::setTexture(effect,"imageTexture",(ID3D11ShaderResourceView*)dens_fb.GetColorTex());
	simul::dx11::UtilityRenderer::DrawQuad2(deviceContext,(2)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("simple"));
	renderPlatform->Print(deviceContext,(2)*(w+8)+8,height-8-w,"dens");
	simul::dx11::setTexture(effect,"imageTexture",(ID3D11ShaderResourceView*)detail_fb.GetColorTex());
	cloud2DConstants.Apply(deviceContext);
	simul::dx11::UtilityRenderer::DrawQuad2(deviceContext,(3)*(w+8)+8,height-8-w,w,w,effect,effect->GetTechniqueByName("show_detail_texture"));
	renderPlatform->Print(deviceContext,(3)*(w+8)+8,height-8-w,"detail");
		
}

void Simul2DCloudRendererDX11::RenderAuxiliaryTextures(crossplatform::DeviceContext &deviceContext,int x0,int y0,int width,int height)
{
}

void Simul2DCloudRendererDX11::SetWindVelocity(float x,float y)
{
}
