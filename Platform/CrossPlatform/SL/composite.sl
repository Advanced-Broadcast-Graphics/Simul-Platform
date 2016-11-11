//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef COMPOSITE_SL
#define COMPOSITE_SL
#define DEBUG_COMPOSITING


#ifndef PI
#define PI (3.1415926536)
#endif

struct TwoColourCompositeOutput
{
	vec4 add		SIMUL_RENDERTARGET_OUTPUT(0);
	vec4 multiply	SIMUL_RENDERTARGET_OUTPUT(1);
};

struct LookupQuad4
{
	vec4 _11;
	vec4 _21;
	vec4 _12;
	vec4 _22;
};

#define VOLUME_INSCATTER
#define SCREENSPACE_VOL
TwoColourCompositeOutput CompositeAtmospherics(vec4 clip_pos
				,TextureCube farCloudTexture
				,TextureCube nearCloudTexture
				,TextureCube nearFarTexture
				,TextureCube lightpassTexture
				,Texture2D loss2dTexture
				,float dist
				,mat4 invViewProj
				,vec3 viewPos
				,Texture3D inscatterVolumeTexture
				,Texture3D godraysVolumeTexture
				,float maxFadeDistanceKm
				,float nearDist
				,bool do_lightpass
				,bool do_godrays)
{
	TwoColourCompositeOutput res;
	vec3 view						=normalize(mul(invViewProj,clip_pos).xyz);
	float sine						=view.z;
	vec4 nearFarCloud				=texture_cube_lod(nearFarTexture	,view		,0);
	
	float dist_rt					=sqrt(dist);
	vec4 cloudFar					=texture_cube_lod(farCloudTexture,view,0);
	vec3 offsetMetres				=view*dist*1000.0*maxFadeDistanceKm;
	vec3 lightspaceOffset			=(mul(worldToScatteringVolumeMatrix,vec4(offsetMetres,1.0)).xyz);
	vec3 worldspaceVolumeTexCoords	=vec3(atan2(view.x,view.y)/(2.0*pi),0.5*(1.0+2.0*asin(sine)/pi),dist_rt);

	// cut-off at the edges.
	vec4 insc						=texture_3d_wmc_lod(inscatterVolumeTexture,worldspaceVolumeTexCoords,0);
	if(do_godrays)
	{
		float r							=length(lightspaceOffset);
		vec3 lightspaceVolumeTexCoords	=vec3(frac(atan2(lightspaceOffset.x,lightspaceOffset.y)/(2.0*pi))
													,0.5+0.5*asin(lightspaceOffset.z/r)*2.0/pi
													,r);
		vec4 godrays					=texture_3d_wcc_lod(godraysVolumeTexture,lightspaceVolumeTexCoords,0);
		insc							*=godrays;
	}
	vec2 loss_texc					=vec2(dist_rt,0.5*(1.f-sine));
	float hiResInterp				=1.0-pow(saturate(( nearFarCloud.x-dist) / max(0.00001,nearFarCloud.x-nearFarCloud.y)),1.0);
	// we're going to do TWO interpolations. One from zero to the near distance,
	// and one from the near to the far distance.
	float nearInterp				=pow(saturate((dist )/0.0033),1.0);
	nearInterp						=saturate((dist-nearDist)/max(0.00000001,2.0*nearDist));
	vec4 lp						=vec4(0,0,0,0);
	if(do_lightpass)
		lp						=texture_cube_lod(lightpassTexture,view,0);
	
	vec4 cloudNear					=texture_cube_lod(nearCloudTexture, view, 0);
	
	vec4 cloud						=lerp(cloudNear, cloudFar,hiResInterp);

	if(do_lightpass)
		cloud.rgb					+=lp.rgb;
	cloud							=lerp(vec4(0, 0, 0, 1.0), cloud, nearInterp);
	
	vec3 worldPos					=viewPos+offsetMetres;
	float illum						=1.0;
	
	insc.rgb						*=cloud.a;

	insc							+=cloud;
	res.multiply					=texture_clamp_mirror_lod(loss2dTexture, loss_texc, 0)*cloud.a;
	res.add							=insc;//vec4(lightspaceVolumeTexCoords,1.0);
/*	
	if(clip_pos.x>.45)
	{
		res.add=nearFarCloud.xxxx;
			res.multiply=vec4(0,0,0,0);
	}
	else if(clip_pos.x>0.3)
	{
		res.add=nearFarCloud.yyyy;
			res.multiply=vec4(0,0,0,0);
	}
	else if(clip_pos.x<-.45)
	{
			res.add=hiResInterp;
	}
	else
	if(clip_pos.x<-.3)
	{
		if(clip_pos.y<0)
		{
			res.multiply=vec4(1,0,0,0);
			res.add=cloudNear;
		}
		else
		{
			res.multiply=vec4(0,1,0,0);
			res.add=cloudFar;
		}
	} */
    return res;
}

TwoColourCompositeOutput CompositeAtmospherics_MSAA(vec4 clip_pos
													,vec2 depth_texc
													,TextureCube farCloudTexture
													,TextureCube nearCloudTexture
													,TextureCube nearFarTexture
													,Texture2D loss2dTexture
													,TEXTURE2DMS_FLOAT4 depthTextureMS
													,int numSamples
													,uint2 fullResDims
													,mat4 invViewProj
													,vec4 viewportToTexRegionScaleBias
													,DepthIntepretationStruct depthInterpretationStruct
													,vec2 lowResTexCoords
													,Texture3D inscatterVolumeTexture
													)
{
	TwoColourCompositeOutput res;
	vec3 view					=mul(invViewProj,clip_pos).xyz;
	vec3 lightspaceView			=normalize((mul(worldToScatteringVolumeMatrix,vec4(view,1.0))).xyz);
#ifdef GLSL
	vec4 zrow					=vec4(invViewProj[0][2],invViewProj[1][2],invViewProj[2][2],invViewProj[3][2]);
#else
	vec4 zrow					=invViewProj._31_32_33_34;
#endif
	int2 fullres_depth_pos2		=int2(depth_texc*vec2(fullResDims.xy));
	
	float sine					=dot(zrow,clip_pos);
	vec4 nearFarCloud			=texture_cube_lod(nearFarTexture	,view		,0);
	
	float dd					=abs(nearFarCloud.x-nearFarCloud.y);
	vec4 cloud					=texture_cube_lod(farCloudTexture,view,0);
	vec4 cloudNear				=texture_cube_lod(nearCloudTexture, view, 0);
	
	float hires_edge			=dd;
	res.add						=vec4(0,0,0,1.0);
	res.multiply				=vec4(0,0,0,0);
	for(int k=0;k<numSamples;k++)
	{
		float depth				=TEXTURE_LOAD_MSAA(depthTextureMS,fullres_depth_pos2,k).x;

		float dist				=depthToLinearDistance(depth	,depthInterpretationStruct);
		float dist_rt			=pow(dist,0.5);
	
		vec3 volumeTexCoords	=vec3(lowResTexCoords, dist_rt);
		vec2 loss_texc			=vec2(dist_rt,0.5*(1.f-sine));
#if 1
		volumeTexCoords			=vec3(atan2(lightspaceView.x,lightspaceView.y)/(2.0*pi),0.5*(1.0+2.0*asin(lightspaceView.z)/pi),dist_rt);
		vec4 insc				=texture_3d_wmc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#else
		vec4 insc				=texture_3d_wwc_lod(inscatterVolumeTexture,volumeTexCoords,0);
#endif
		float hiResInterp		=saturate((dist - nearFarCloud.y) / max(dd,0.000001));
		// we're going to do TWO interpolations. One from zero to the near distance,
		// and one from the near to the far distance.
		float nearInterp		=saturate(dist / max(nearFarCloud.y,0.000001));
	
		cloud					=lerp(cloudNear, cloud, hiResInterp);
	
		cloud					=lerp(vec4(0,0,0,1.0),cloud,nearInterp);
		insc.rgb				*=cloud.a;
		insc					+=cloud;
		res.multiply			+=texture_wrap_lod(loss2dTexture,loss_texc,0)*cloud.a;
		res.add					+=insc;
	}
	res.multiply/=float(numSamples);
	res.add/=float(numSamples);
    return res;
}
#endif
