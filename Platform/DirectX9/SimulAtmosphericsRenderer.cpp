// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulAtmosphericsRenderer.cpp A renderer for skies, clouds and weather effects.

#include "SimulAtmosphericsRenderer.h"

#ifdef XBOX
	#include <xgraphics.h>
	#include <fstream>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
#else
	#include <tchar.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
#endif
#include "CreateDX9Effect.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Macros.h"
#include "Resources.h"

SimulAtmosphericsRenderer::SimulAtmosphericsRenderer()
	:m_pd3dDevice(NULL)
	,vertexDecl(NULL)
	,effect(NULL)
	,lightDir(NULL)
	,cloudScales(NULL)
	,cloudOffset(NULL)
	,lightColour(NULL)
	,eyePosition(NULL)
	,cloudInterp(NULL)
	,cloudTexture1(NULL)
	,cloudTexture2(NULL)
	,mieRayleighRatio(NULL)
	,hazeEccentricity(NULL)
	,fadeInterp(NULL)
	,imageTexture(NULL)
	,depthTexture(NULL)
	,lossTexture1(NULL)
	,inscatterTexture1(NULL)
	,loss_texture_1(NULL)
	,inscatter_texture_1(NULL)
	,fade_interp(0.f)
	,altitude_tex_coord(0.f)
	,input_texture(NULL)
	,max_distance_texture(NULL)
	,lightning_illumination_texture(NULL)
	,y_vertical(true)
{
}

bool SimulAtmosphericsRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	HRESULT hr=S_OK;
	RecompileShaders();
	// For a HUD, we use D3DDECLUSAGE_POSITIONT instead of D3DDECLUSAGE_POSITION
	D3DVERTEXELEMENT9 decl[] = 
	{
#ifdef XBOX
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0,  12, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#else
		{ 0,  0, D3DDECLTYPE_FLOAT3		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2		,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0 },
#endif
		D3DDECL_END()
	};
	SAFE_RELEASE(vertexDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&vertexDecl);
	return (hr==S_OK);
}

void SimulAtmosphericsRenderer::RecompileShaders()
{
	SAFE_RELEASE(effect);
	std::map<std::string,std::string> defines;
	defines["WRAP_CLOUDS"]="1";
	if(!y_vertical)
		defines["Z_VERTICAL"]='1';
	else
		defines["Y_VERTICAL"]='1';
	V_CHECK(CreateDX9Effect(m_pd3dDevice,effect,"atmospherics.fx",defines));

	technique			=effect->GetTechniqueByName("simul_atmospherics");
	godRaysTechnique	=effect->GetTechniqueByName("simul_godrays");
	airglowTechnique	=effect->GetTechniqueByName("simul_airglow");

	invViewProj			=effect->GetParameterByName(NULL,"invViewProj");
	altitudeTexCoord	=effect->GetParameterByName(NULL,"altitudeTexCoord");


	heightAboveFogLayer	=effect->GetParameterByName(NULL,"heightAboveFogLayer");
	fogColour			=effect->GetParameterByName(NULL,"fogColour");

	fogScaleHeight		=effect->GetParameterByName(NULL,"fogScaleHeight");
	fogDensity			=effect->GetParameterByName(NULL,"fogDensity");


	lightDir			=effect->GetParameterByName(NULL,"lightDir");
	mieRayleighRatio	=effect->GetParameterByName(NULL,"mieRayleighRatio");
	hazeEccentricity	=effect->GetParameterByName(NULL,"hazeEccentricity");
	fadeInterp			=effect->GetParameterByName(NULL,"fadeInterp");
	imageTexture		=effect->GetParameterByName(NULL,"imageTexture");
	depthTexture		=effect->GetParameterByName(NULL,"depthTexture");
	lossTexture1		=effect->GetParameterByName(NULL,"lossTexture1");
	inscatterTexture1	=effect->GetParameterByName(NULL,"inscatterTexture1");

	maxDistanceTexture	=effect->GetParameterByName(NULL,"maxDistanceTexture");

	cloudScales			=effect->GetParameterByName(NULL,"cloudScales");
	cloudOffset			=effect->GetParameterByName(NULL,"cloudOffset");
	lightColour			=effect->GetParameterByName(NULL,"lightColour");
	eyePosition			=effect->GetParameterByName(NULL,"eyePosition");
	cloudInterp			=effect->GetParameterByName(NULL,"cloudInterp");
	texelOffsets		=effect->GetParameterByName(NULL,"texelOffsets");

	
	cloudTexture1		=effect->GetParameterByName(NULL,"cloudTexture1");
	cloudTexture2		=effect->GetParameterByName(NULL,"cloudTexture2");


	lightningIlluminationTexture	=effect->GetParameterByName(NULL,"lightningIlluminationTexture");
	lightningMultipliers			=effect->GetParameterByName(NULL,"lightningMultipliers");
	lightningColour					=effect->GetParameterByName(NULL,"lightningColour");
	illuminationOrigin				=effect->GetParameterByName(NULL,"illuminationOrigin");
	illuminationScales				=effect->GetParameterByName(NULL,"illuminationScales");
}

bool SimulAtmosphericsRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	SAFE_RELEASE(vertexDecl);
	if(effect)
        hr=effect->OnLostDevice();
	SAFE_RELEASE(effect);
	return (hr==S_OK);
}

SimulAtmosphericsRenderer::~SimulAtmosphericsRenderer()
{
	InvalidateDeviceObjects();
}

#ifdef XBOX
void SimulAtmosphericsRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}
#endif
void SimulAtmosphericsRenderer::SetCloudProperties(void* c1,void* c2,
			const float *cloudscales,const float *cloudoffset,float interp)
{
	cloud_texture1=(LPDIRECT3DBASETEXTURE9)c1;
	cloud_texture2=(LPDIRECT3DBASETEXTURE9)c2;
	cloud_scales=cloudscales;
	cloud_offset=cloudoffset;
	cloud_interp=interp;
}
void SimulAtmosphericsRenderer::SetLightningProperties(	void *tex,
		simul::clouds::LightningRenderInterface *lri)
{
	lightning_illumination_texture=(LPDIRECT3DBASETEXTURE9)tex;
	for(int i=0;i<4;i++)
	{
		if(i<(int)lri->GetNumLightSources())
			(lightning_multipliers.operator float *())[i]=lri->GetLightSourceBrightness(i);
		else
			(lightning_multipliers.operator float *())[i]=0;
	}
	illumination_scales=lri->GetIlluminationScales();
	illumination_scales.x=1.f/illumination_scales.x;
	illumination_scales.y=1.f/illumination_scales.y;
	illumination_scales.z=1.f/illumination_scales.z;
	illumination_offset=lri->GetIlluminationOrigin();
	lightning_colour=lri->GetLightningColour();
}

static D3DXVECTOR4 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR4 dcam_pos;
	dcam_pos.x=tmp1._41;
	dcam_pos.y=tmp1._42;
	dcam_pos.z=tmp1._43;
	return dcam_pos;
}

bool SimulAtmosphericsRenderer::RenderGodRays(float strength)
{
	if(strength<=0.f)
		return true;
	HRESULT hr=S_OK;
	effect->SetTechnique(godRaysTechnique);
	PIXWrapper(0xFFFFFF00,"Render Godrays")
	{
#ifndef XBOX
		m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
		m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
		D3DXVECTOR4 cam_pos=GetCameraPosVector(view);
		hr=effect->SetFloat(fadeInterp,fade_interp);
		hr=effect->SetTexture(imageTexture,input_texture);
		hr=effect->SetFloat(altitudeTexCoord,altitude_tex_coord);
		if(skyInterface)
		{
			hr=effect->SetFloat(hazeEccentricity,skyInterface->GetMieEccentricity());
			D3DXVECTOR4 mie_rayleigh_ratio(skyInterface->GetMieRayleighRatio());
			D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
			D3DXVECTOR4 light_colour(skyInterface->GetLocalIrradiance(cam_pos.y));
			
			light_colour*=strength;
			if(y_vertical)
				std::swap(sun_dir.y,sun_dir.z);

			effect->SetVector	(lightDir			,&sun_dir);
			effect->SetVector	(lightColour	,(const D3DXVECTOR4*)&light_colour);
		}
		hr=effect->SetTexture(maxDistanceTexture,max_distance_texture);
		hr=effect->SetTexture(cloudTexture1,cloud_texture1);	
		hr=effect->SetTexture(cloudTexture2,cloud_texture2);
		effect->SetVector	(cloudScales	,(const D3DXVECTOR4*)&cloud_scales);
		effect->SetVector	(cloudOffset	,(const D3DXVECTOR4*)&cloud_offset);
		effect->SetVector	(eyePosition	,&cam_pos);
		effect->SetFloat	(cloudInterp	,cloud_interp);

		hr=DrawScreenQuad();
	}
	return (hr==S_OK);
}

bool SimulAtmosphericsRenderer::RenderAirglow()
{
	HRESULT hr=S_OK;
	if(simul::sky::length(lightning_multipliers)==0)
		return true;
	effect->SetTechnique(airglowTechnique);
	PIXWrapper(0xFFFFFF00,"Render Airglow")
	{
#ifndef XBOX
		m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
		m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
		D3DXVECTOR4 cam_pos=GetCameraPosVector(view);
		hr=effect->SetTexture(lightningIlluminationTexture,lightning_illumination_texture);
		hr=effect->SetTexture(maxDistanceTexture,max_distance_texture);
		effect->SetVector	(lightningColour		,(const D3DXVECTOR4*)&lightning_colour);
		effect->SetVector	(illuminationScales		,(const D3DXVECTOR4*)&illumination_scales);
		effect->SetVector	(illuminationOrigin		,(const D3DXVECTOR4*)&illumination_offset);
		effect->SetVector	(lightningMultipliers	,(const D3DXVECTOR4*)&lightning_multipliers);
		effect->SetVector	(eyePosition			,&cam_pos);
		hr=DrawScreenQuad();
	}
	return (hr==S_OK);
}

bool SimulAtmosphericsRenderer::DrawScreenQuad()
{
	HRESULT hr=S_OK;
#ifndef XBOX
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);
#endif
	D3DXMATRIX vpt;
	D3DXMATRIX viewproj;
	view._41=view._42=view._43=0;
	D3DXMatrixMultiply(&viewproj,&view,&proj);
	D3DXMatrixTranspose(&vpt,&viewproj);
	D3DXMATRIX ivp;
	D3DXMatrixInverse(&ivp,NULL,&vpt);
	
	if(input_texture)
	{
		D3DSURFACE_DESC desc;
		input_texture->GetLevelDesc(0,&desc);
		D3DXVECTOR4 vec(1.f/(float)desc.Width,1.f/(float)desc.Height,0,0);
		effect->SetVector(texelOffsets,&vec);
	}

	hr=effect->SetMatrix(invViewProj,&ivp);

	hr=DrawFullScreenQuad(m_pd3dDevice,effect);
	return (hr==S_OK);
}

bool SimulAtmosphericsRenderer::Render()
{
	HRESULT hr=S_OK;
	effect->SetTechnique(technique);
	PIXWrapper(0xFFFFFF00,"Render Atmospherics")
	{
		hr=effect->SetFloat(fadeInterp,fade_interp);
		hr=effect->SetTexture(imageTexture,input_texture);
		hr=effect->SetFloat(altitudeTexCoord,altitude_tex_coord);

#ifndef XBOX
		m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
#endif
		D3DXVECTOR4 cam_pos=GetCameraPosVector(view);
		float h=y_vertical?cam_pos.y:cam_pos.z;
		hr=effect->SetFloat(heightAboveFogLayer,(h/1000.f-fog_height_km)/(fade_distance_km));
		hr=effect->SetFloat(fogScaleHeight,fog_scale_height_km/(fade_distance_km));
		hr=effect->SetVector(fogColour,(D3DXVECTOR4*)(&fog_colour));
		hr=effect->SetFloat(fogDensity,fog_dens);
		if(skyInterface)
		{
			hr=effect->SetFloat(hazeEccentricity,skyInterface->GetMieEccentricity());
			D3DXVECTOR4 mie_rayleigh_ratio(skyInterface->GetMieRayleighRatio());
			D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
			if(y_vertical)
				std::swap(sun_dir.y,sun_dir.z);
			effect->SetVector	(lightDir			,&sun_dir);
			effect->SetVector	(mieRayleighRatio	,&mie_rayleigh_ratio);
		}
		hr=effect->SetTexture(lossTexture1,loss_texture_1);
		hr=effect->SetTexture(inscatterTexture1,inscatter_texture_1);
		hr=effect->SetTexture(maxDistanceTexture,max_distance_texture);
		hr=DrawScreenQuad();
	}
	return (hr==S_OK);
}
