#define NOMINMAX
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "GpuCloudGenerator.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "D3dx11effect.h"
#include <math.h>

using namespace simul;
using namespace dx11;

GpuCloudGenerator::GpuCloudGenerator()
			:m_pd3dDevice(NULL)
			,m_pImmediateContext(NULL)
			,effect(NULL)
			,densityComputeTechnique(NULL)
			,lightingComputeTechnique(NULL)
			,secondaryLightingComputeTechnique(NULL)
			,transformComputeTechnique(NULL)
			,volume_noise_tex(NULL)
			,volume_noise_tex_srv(NULL)
			,m_pWwcSamplerState(NULL)
			,m_pCwcSamplerState(NULL)
			,m_pWccSamplerState(NULL)
	,harmonic_secondary(false)
{
	for(int i=0;i<3;i++)
		finalTexture[i]=NULL;
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}

void GpuCloudGenerator::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
	// Mask must have depth as that's how it merges.
	mask_fb.SetDepthFormat(crossplatform::D_32_FLOAT);
	mask_fb.RestoreDeviceObjects(renderPlatform);
	gpuCloudConstants.RestoreDeviceObjects(m_pd3dDevice);
	SAFE_RELEASE(m_pWwcSamplerState);
	SAFE_RELEASE(m_pCwcSamplerState);
	SAFE_RELEASE(m_pWccSamplerState);
	D3D11_SAMPLER_DESC samplerDesc;
	
    ZeroMemory( &samplerDesc, sizeof( D3D11_SAMPLER_DESC ) );
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC   ;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pWwcSamplerState);
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pCwcSamplerState);
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	m_pd3dDevice->CreateSamplerState(&samplerDesc,&m_pWccSamplerState);

	RecompileShaders();
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	mask_fb.InvalidateDeviceObjects();
	SAFE_RELEASE(volume_noise_tex);
	SAFE_RELEASE(volume_noise_tex_srv);
	SAFE_RELEASE(m_pImmediateContext);
	SAFE_DELETE(effect);
	density_texture.InvalidateDeviceObjects();
	gpuCloudConstants.InvalidateDeviceObjects();
	for(int i=0;i<2;i++)
	{
		directLightTextures[i].InvalidateDeviceObjects();
		indirectLightTextures[i].InvalidateDeviceObjects();
	}
	SAFE_RELEASE(m_pWwcSamplerState);
	SAFE_RELEASE(m_pCwcSamplerState);
	SAFE_RELEASE(m_pWccSamplerState);
	m_pd3dDevice=NULL;
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_DELETE(effect);
	effect=renderPlatform->CreateEffect("simul_gpu_clouds");
	if(effect->asD3DX11Effect())
	{
		lightingComputeTechnique			=effect->GetTechniqueByName("gpu_lighting_compute");
		secondaryLightingComputeTechnique	=effect->GetTechniqueByName("gpu_secondary_compute");
		secondaryHarmonicTechnique			=effect->GetTechniqueByName("gpu_secondary_harmonic");
		maskTechnique						=effect->GetTechniqueByName("density_mask");
		densityComputeTechnique				=effect->GetTechniqueByName("gpu_density_compute");
		transformComputeTechnique			=effect->GetTechniqueByName("gpu_transform_compute");
	}
	gpuCloudConstants.LinkToEffect(effect->asD3DX11Effect(),"GpuCloudConstants");
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	//if(iformat==DXGI_FORMAT_R32G32B32A32_FLOAT)
	//	size=4;
	return grid[0]*grid[1]*grid[2];
}

void* GpuCloudGenerator::Make3DNoiseTexture(int noise_size,const float *noise_src_ptr,int generation_number)
{
	if(last_generation_number==generation_number&&volume_noise_tex_srv!=NULL)
		return volume_noise_tex_srv;
	noiseSize=noise_size;
	//using noise_size and noise_src_ptr, make a 3d texture:
	SAFE_RELEASE(volume_noise_tex);
	SAFE_RELEASE(volume_noise_tex_srv);
	volume_noise_tex=make3DTexture(m_pd3dDevice,noise_size,noise_size,noise_size,DXGI_FORMAT_R32_FLOAT,noise_src_ptr);
	m_pd3dDevice->CreateShaderResourceView(volume_noise_tex,NULL,&volume_noise_tex_srv);
	//m_pImmediateContext->GenerateMips(volume_noise_tex_srv);
	last_generation_number=generation_number;
	return volume_noise_tex_srv;
}

void GpuCloudGenerator::FillDensityGrid(int index
										,const clouds::GpuCloudsParameters &params
										,int start_texel
										,int texels)
{
	if(texels<=0)
		return;
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context	=m_pImmediateContext;
	deviceContext.renderPlatform	=renderPlatform;

	for(int i=0;i<3;i++)
		finalTexture[i]->ensureTexture3DSizeAndFormat(renderPlatform,params.density_grid[0],params.density_grid[1],params.density_grid[2],crossplatform::RGBA_8_UNORM,true,1);
	int density_gridsize=params.density_grid[0]*params.density_grid[1]*params.density_grid[2];
	mask_fb.SetWidthAndHeight(params.density_grid[0],params.density_grid[1]);

	mask_fb.Activate(deviceContext);
	const simul::clouds::MaskMap &masks=*params.masks;
	if(masks.size())
	{
		mask_fb.Clear(m_pImmediateContext,0.f,0.f,0.f,0.f,0.f);
		
		for(simul::clouds::MaskMap::const_iterator i=masks.begin();i!=masks.end();i++)
		{
			gpuCloudConstants.yRange		=vec4(0,1.f,0,0);
			gpuCloudConstants.maskCentre	=vec2(i->second.x,i->second.y);
			gpuCloudConstants.maskRadius	=i->second.radius;
			gpuCloudConstants.maskFeather	=0.1f;
			gpuCloudConstants.maskThickness	=i->second.thickness;
			gpuCloudConstants.Apply(deviceContext);
			effect->Apply(deviceContext,maskTechnique,0);
			simul::dx11::UtilityRenderer::DrawQuad(deviceContext);
			effect->Unapply(deviceContext);
		}
	}
	else
	{
		mask_fb.Clear(m_pImmediateContext,1.f,1.f,1.f,1.f,1.f);
	}
	mask_fb.Deactivate(deviceContext);

	int z0	=start_texel/(params.density_grid[0]*params.density_grid[1]);
	int z1	=(start_texel+texels)/(params.density_grid[0]*params.density_grid[1]);
	int zmax=params.density_grid[2];
	float y_start					=(float)z0/(float)zmax;
	float y_range					=(float)z1/(float)zmax-y_start;
	SetGpuCloudConstants(gpuCloudConstants,params,y_start,y_range);

	simul::dx11::setTexture(effect->asD3DX11Effect(),"volumeNoiseTexture"	,volume_noise_tex_srv);
	simul::dx11::setTexture(effect->asD3DX11Effect(),"maskTexture"			,(ID3D11ShaderResourceView*)mask_fb.GetColorTex());
	
	density_texture.ensureTexture3DSizeAndFormat(renderPlatform
		,params.density_grid[0],params.density_grid[1],params.density_grid[2]
		,crossplatform::R_32_FLOAT,true);

	simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTexture",density_texture.AsD3D11UnorderedAccessView());

	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(params.density_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(params.density_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=(params.density_grid[2]+BLOCKWIDTH-1)/BLOCKWIDTH;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;
	// which blocks to execute?
	int x0	=start_texel/BLOCKSIZE/subgrid.y/subgrid.z;
	int x1	=(((start_texel+texels+BLOCKSIZE-1)/(BLOCKSIZE)+subgrid.y-1)/subgrid.y+subgrid.z-1)/subgrid.z;
	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(deviceContext);
	effect->Apply(deviceContext,densityComputeTechnique,0);
	if(x1>x0)
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,subgrid.z);
	simul::dx11::setTexture				(effect->asD3DX11Effect(),"volumeNoiseTexture"	,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setTexture				(effect->asD3DX11Effect(),"maskTexture"			,(ID3D11ShaderResourceView*)NULL);
	simul::dx11::setUnorderedAccessView	(effect->asD3DX11Effect(),"targetTexture"		,(ID3D11UnorderedAccessView*)NULL);
	effect->Unapply(deviceContext);
}

void GpuCloudGenerator::PerformGPURelight	(int light_index
											,const clouds::GpuCloudsParameters &params
											,float *target
											,int start_texel
											,int texels)
{
	if(texels<=0)
		return;
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context=m_pImmediateContext;
	start_texel*=2;
	texels*=2;
	const int *light_grid=NULL;
	if(light_index==0)
		light_grid=params.density_grid;
	else
		light_grid=params.light_grid;
	int gridsize=light_grid[0]*light_grid[1]*light_grid[2];
	if(start_texel<0)
		start_texel=0;
	if(start_texel>gridsize)
		start_texel=gridsize;	
	if(start_texel+texels>gridsize)
		texels=gridsize-start_texel; 
	directLightTextures[light_index].ensureTexture3DSizeAndFormat(renderPlatform
				,light_grid[0],light_grid[1],light_grid[2]
				,crossplatform::R_32_FLOAT,true);
	indirectLightTextures[light_index].ensureTexture3DSizeAndFormat(renderPlatform
				,light_grid[0],light_grid[1],light_grid[2]
				,crossplatform::R_32_FLOAT,true);

	ID3D1xEffectShaderResourceVariable*	densityTexture		=effect->asD3DX11Effect()->GetVariableByName("densityTexture")->AsShaderResource();
	//SetGpuCloudConstants(gpuCloudConstants);
	gpuCloudConstants.yRange			=vec4(0.0,1.0,0,0);
	if(light_index==0)
	{
		gpuCloudConstants.extinctions		=vec2(params.lightspace_extinctions[2],params.lightspace_extinctions[3]);
		gpuCloudConstants.transformMatrix	=params.Matrix4x4AmbientToDensityTexcoords;
	}
	else
	{
		gpuCloudConstants.extinctions		=vec2(params.lightspace_extinctions[0],params.lightspace_extinctions[1]);
		gpuCloudConstants.transformMatrix	=params.Matrix4x4LightToDensityTexcoords;
	}
	gpuCloudConstants.transformMatrix.transpose();
	gpuCloudConstants.zPixelLightspace	=(1.f/(float)light_grid[2]);

	//transformMatrix * (0,0,1)
	simul::sky::float4 step(gpuCloudConstants.transformMatrix._31,gpuCloudConstants.transformMatrix._32,gpuCloudConstants.transformMatrix._33,0);
	// We require stepLength to be the distance in km of each step along the light path.
	// So we divide by the light texel count in the light direction, then multiply by the three density axis scales.
	step*=gpuCloudConstants.zPixelLightspace;
	step*=simul::sky::float4(params.DensityGridScalesM[0],params.DensityGridScalesM[1],params.DensityGridScalesM[2],1.0);
	gpuCloudConstants.stepLength		=simul::sky::length(step); 
	if(params.wrap_light_tex[light_index])
		simul::dx11::setSamplerState(effect->asD3DX11Effect(),"lightSamplerState",m_pWwcSamplerState);
	else if(light_grid[0]>light_grid[1])
		simul::dx11::setSamplerState(effect->asD3DX11Effect(),"lightSamplerState",m_pWccSamplerState);
	else
		simul::dx11::setSamplerState(effect->asD3DX11Effect(),"lightSamplerState",m_pCwcSamplerState);
	densityTexture->SetResource(density_texture.AsD3D11ShaderResourceView());

	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(light_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(light_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=(light_grid[2]+BLOCKWIDTH-1)/BLOCKWIDTH;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;

	// discard z dimension:
	int t0	=(start_texel)/light_grid[2];
	int t1	=(start_texel+texels+light_grid[2]-1)/light_grid[2];
	int t	=t1-t0;
	// which blocks to execute?
	int x0	=t0/BLOCKSIZE/subgrid.y;
	int x1	=((t0+t+BLOCKSIZE-1)/BLOCKSIZE+subgrid.y-1)/subgrid.y;
	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(deviceContext);
	if(x1>x0)
	{
		simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTexture1",directLightTextures[light_index].AsD3D11UnorderedAccessView());
	densityTexture->SetResource(density_texture.AsD3D11ShaderResourceView());
		effect->Apply(deviceContext,lightingComputeTechnique,0);
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,1);
		effect->Unapply(deviceContext);
	}
	int z0	=start_texel/light_grid[1]/light_grid[0];
	int z1	=(start_texel+texels+light_grid[1]*light_grid[0]-1)/light_grid[1]/light_grid[0];
	if(z1>z0)
	{
		if(harmonic_secondary)
		{
			gpuCloudConstants.threadOffset=uint3(0,0,0);
			gpuCloudConstants.Apply(deviceContext);
			setTexture(effect->asD3DX11Effect(),"lightTexture1"				,directLightTextures[light_index].AsD3D11ShaderResourceView());
			simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTexture1",indirectLightTextures[light_index].AsD3D11UnorderedAccessView());
	densityTexture->SetResource(density_texture.AsD3D11ShaderResourceView());
			effect->Apply(deviceContext,secondaryHarmonicTechnique,0);
			m_pImmediateContext->Dispatch(subgrid.x,subgrid.y,subgrid.z);
			simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTexture1",(ID3D11UnorderedAccessView*)NULL);
	densityTexture->SetResource(NULL);
		effect->Unapply(deviceContext);
		}
		else
		for(int z=z0;z<z1;z++)
		{
			gpuCloudConstants.threadOffset=uint3(0,0,z);
			gpuCloudConstants.Apply(deviceContext);
			simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTexture1",indirectLightTextures[light_index].AsD3D11UnorderedAccessView());
			setTexture(effect->asD3DX11Effect(),"lightTexture1"				,directLightTextures[light_index].AsD3D11ShaderResourceView());
	densityTexture->SetResource(density_texture.AsD3D11ShaderResourceView());
			effect->Apply(deviceContext,secondaryLightingComputeTechnique,0);
			m_pImmediateContext->Dispatch(subgrid.x,subgrid.y,1);
			simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTexture1",(ID3D11UnorderedAccessView*)NULL);
	densityTexture->SetResource(NULL);
			effect->Unapply(deviceContext);
		}
	}

	// copy to CPU memory if required by CloudKeyframer.
	if(target)
	{
		directLightTextures[light_index].copyToMemory(deviceContext,target,start_texel,texels);
		target+=gridsize;
		indirectLightTextures[light_index].copyToMemory(deviceContext,target,start_texel,texels);
	}
}

void GpuCloudGenerator::GPUTransferDataToTexture(int cycled_index
												,const clouds::GpuCloudsParameters &params
												,unsigned char *target
												,int start_texel
												,int texels)
{
	if(texels<=0)
		return;
	crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context=m_pImmediateContext;
	int density_gridsize				=params.density_grid[0]*params.density_grid[1]*params.density_grid[2];

	int z0								=start_texel/(params.density_grid[0]*params.density_grid[1]);
	int z1								=(start_texel+texels)/(params.density_grid[0]*params.density_grid[1]);
	int zmax							=params.density_grid[2];

	float y_start						=(float)z0/(float)zmax;
	float y_range						=(float)z1/(float)zmax-y_start;
	gpuCloudConstants.yRange			=vec4(y_start,y_range,0,0);
	gpuCloudConstants.transformMatrix	=params.Matrix4x4DensityToLightTransform;
	gpuCloudConstants.transformMatrix.transpose();

	gpuCloudConstants.zSize				=((float)params.density_grid[2]);
	gpuCloudConstants.zPixel			=(1.f/(float)params.density_grid[2]);
	gpuCloudConstants.zPixelLightspace	=(1.f/(float)params.light_grid[2]);

	setTexture(effect->asD3DX11Effect(),"densityTexture"		,density_texture.AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"ambientTexture1"		,directLightTextures[0].AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"ambientTexture2"		,indirectLightTextures[0].AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"lightTexture1"		,directLightTextures[1].AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"lightTexture2"		,indirectLightTextures[1].AsD3D11ShaderResourceView());
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	gpuCloudConstants.Apply(deviceContext);
	for(int i=0;i<3;i++)
	{
		finalTexture[i]->ensureTexture3DSizeAndFormat(renderPlatform,params.density_grid[0],params.density_grid[1],params.density_grid[2],crossplatform::RGBA_8_UNORM,true);
	}
	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(params.density_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(params.density_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=(params.density_grid[2]+BLOCKWIDTH-1)/BLOCKWIDTH;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;
	// which blocks to execute?
	int x0	=start_texel/BLOCKSIZE/subgrid.y/subgrid.z;
	int x1	=(((start_texel+texels+BLOCKSIZE-1)/(BLOCKSIZE)+subgrid.y-1)/subgrid.y+subgrid.z-1)/subgrid.z;

	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(deviceContext);
	//simul::dx11::setParameter(effect->asD3DX11Effect(),"targetTexture",density_texture.AsD3D11UnorderedAccessView());
	effect->SetUnorderedAccessView(deviceContext,"targetTexture",finalTexture[cycled_index]);
	effect->Apply(deviceContext,transformComputeTechnique,0);
	if(x1>x0)
		m_pImmediateContext->Dispatch(x1-x0,subgrid.y,subgrid.z);
	simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"targetTexture",(ID3D11UnorderedAccessView*)NULL);
	setTexture(effect->asD3DX11Effect(),"densityTexture"	,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"ambientTexture1"	,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"ambientTexture2"	,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"lightTexture1"		,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"lightTexture2"		,(ID3D11ShaderResourceView*)NULL);
		effect->Unapply(deviceContext);
	// copy to CPU memory if required by CloudKeyframer.
	if(target)
	{
		finalTexture[cycled_index]->copyToMemory(deviceContext,target,start_texel,texels);
	}
}