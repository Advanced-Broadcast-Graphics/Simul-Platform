#ifndef MIXED_RESOLUTION_SL
#define MIXED_RESOLUTION_SL

void Resolve(Texture2DMS<float4> sourceTextureMS,RWTexture2D<float4> targetTexture,uint2 pos)
{
	uint2 source_dims;
	uint numberOfSamples;
	sourceTextureMS.GetDimensions(source_dims.x,source_dims.y,numberOfSamples);
	uint2 dims;
	targetTexture.GetDimensions(dims.x,dims.y);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	vec4 d=vec4(0,0,0,0);
	for(uint k=0;k<numberOfSamples;k++)
	{
		d+=sourceTextureMS.Load(pos,k);
	}
	d/=float(numberOfSamples);
	targetTexture[pos.xy]	=d;
}

// Find nearest and furthest depths in MSAA texture.
// sourceDepthTexture, sourceMSDepthTexture, and targetTexture are ALL the SAME SIZE.
vec4 MakeDepthFarNear(Texture2D<float4> sourceDepthTexture,Texture2DMS<float4> sourceMSDepthTexture,uint numberOfSamples,uint2 pos,vec4 depthToLinFadeDistParams)
{
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	for(uint k=0;k<numberOfSamples;k++)
	{
		float d;
		if(numberOfSamples==1)
			d				=sourceDepthTexture[pos].x;
		else
			d				=sourceMSDepthTexture.Load(pos,k).x;
#if REVERSE_DEPTH==1
		if(d>nearest_depth)
			nearest_depth	=d;
		if(d<farthest_depth)
			farthest_depth	=d;
#else
		if(d<nearest_depth)
			nearest_depth	=d;
		if(d>farthest_depth)
			farthest_depth	=d;
#endif
	}
	float edge		=0.0;
	if(farthest_depth!=nearest_depth)
	{
		vec2 fn		=depthToLinearDistance(vec2(farthest_depth,nearest_depth),depthToLinFadeDistParams);
		edge		=fn.x-fn.y;
		edge		=step(0.002,edge);
	}
	return vec4(farthest_depth,nearest_depth,edge,0.0);
}

vec4 DownscaleDepthFarNear(Texture2D<float4> sourceDepthTexture,uint2 source_dims,uint2 pos,uint2 scale,vec4 depthToLinFadeDistParams)
{
	// pos is the texel position in the target.
	uint2 pos2=pos*scale;
	// now pos2 is the texel position in the source.
	// scale must represent the exact number of horizontal and vertical pixels for the hi-res texture that fit into each texel of the downscaled texture.
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	for(uint i=0;i<scale.x;i++)
	{
		for(uint j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=pos2+uint2(i,j);
			if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
				continue;
			float d				=sourceDepthTexture[hires_pos].x;
#if REVERSE_DEPTH==1
			if(d>nearest_depth)
				nearest_depth	=d;
			if(d<farthest_depth)
				farthest_depth	=d;
#else
			if(d<nearest_depth)
				nearest_depth	=d;
			if(d>farthest_depth)
				farthest_depth	=d;
#endif
		}
	}
	float edge=0.0;
	if(nearest_depth!=farthest_depth)
	{
		float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
		float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
		edge		=f-n;
		edge		=step(0.002,edge);
	}
	vec4 res=vec4(farthest_depth,nearest_depth,edge,0);
	return res;
}

vec4 DownscaleDepthFarNear_MSAA(Texture2DMS<float4> sourceMSDepthTexture,uint2 source_dims,int numberOfSamples,uint2 pos,vec2 scale,vec4 depthToLinFadeDistParams)
{
	// scale must represent the exact number of horizontal and vertical pixels for the multisampled texture that fit into each texel of the downscaled texture.
	uint2 pos2					=pos*scale;
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	for(uint i=0;i<scale.x;i++)
	{
		for(uint j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=pos2+uint2(i,j);
			if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
				continue;
			for(int k=0;k<numberOfSamples;k++)
			{
				float d				=sourceMSDepthTexture.Load(hires_pos,k).x;
#if REVERSE_DEPTH==1
				if(d>nearest_depth)
					nearest_depth	=d;
				if(d<farthest_depth)
					farthest_depth	=d;
#else
				if(d<nearest_depth)
					nearest_depth	=d;
				if(d>farthest_depth)
					farthest_depth	=d;
#endif
			}
		}
	}
	float edge=0.0;
	if(nearest_depth!=farthest_depth)
	{
	float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
	float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
		edge	=f-n;
		edge	=step(0.002,edge);
	}
	return		vec4(farthest_depth,nearest_depth,edge,0.0);
}

void SpreadEdge(Texture2D<vec4> sourceDepthTexture,RWTexture2D<vec4> target2DTexture,uint2 pos)
{
	float e=0.0;
	for(int i=-1;i<2;i++)
	{
		for(int j=-1;j<2;j++)
		{
			e=max(e,sourceDepthTexture[pos.xy+uint2(i,j)].z);
		}
	}
	vec4 res=sourceDepthTexture[pos.xy];
	res.z=e;
	target2DTexture[pos.xy]=res;
}

vec4 DownscaleFarNearEdge(Texture2D<float4> sourceDepthTexture,uint2 source_dims,uint2 pos,uint2 scale,vec4 depthToLinFadeDistParams)
{
	// pos is the texel position in the target.
	uint2 pos2=pos*scale;
	// now pos2 is the texel position in the source.
	// scale must represent the exact number of horizontal and vertical pixels for the hi-res texture that fit into each texel of the downscaled texture.
#if REVERSE_DEPTH==1
	float nearest_depth			=0.0;
	float farthest_depth		=1.0;
#else
	float nearest_depth			=1.0;
	float farthest_depth		=0.0;
#endif
	vec4 res=vec4(0,0,0,0);
	for(uint i=0;i<scale.x;i++)
	{
		for(uint j=0;j<scale.y;j++)
		{
			uint2 hires_pos		=pos2+uint2(i,j);
			if(hires_pos.x>=source_dims.x||hires_pos.y>=source_dims.y)
				continue;
			vec4 d				=sourceDepthTexture[hires_pos];
#if REVERSE_DEPTH==1
			if(d.y>nearest_depth)
				nearest_depth	=d.y;
			if(d.x<farthest_depth)
				farthest_depth	=d.x;
#else
			if(d.y<nearest_depth)
				nearest_depth	=d.y;
			if(d.x>farthest_depth)
				farthest_depth	=d.x;
#endif
		}
	}
	float edge=0.0;
	if(nearest_depth!=farthest_depth)
	{
		float n		=depthToLinearDistance(nearest_depth,depthToLinFadeDistParams);
		float f		=depthToLinearDistance(farthest_depth,depthToLinFadeDistParams);
		edge		=f-n;
		edge		=step(0.002,edge);
	}
	res=vec4(farthest_depth,nearest_depth,edge,0);
	return res;
}
#ifndef GLSL


// Filter the texture, but bias the result towards the nearest depth values.
vec4 depthDependentFilteredImage(Texture2D imageTexture
								 ,Texture2D fallbackTexture
								 ,Texture2D depthTexture
								 ,vec2 lowResDims
								 ,vec2 texc
								 ,vec4 depthMask
								 ,vec4 depthToLinFadeDistParams
								 ,float d
								 ,bool do_fallback)
{
#if 0
	return texture_clamp_lod(imageTexture,texc,0);
#else
	vec2 texc_unit	=texc*lowResDims-vec2(.5,.5);
	uint2 idx		=floor(texc_unit);
	vec2 xy			=frac(texc_unit);
	int i1			=max(0,idx.x);
	int i2			=min(idx.x+1,lowResDims.x-1);
	int j1			=max(0,idx.y);
	int j2			=min(idx.y+1,lowResDims.y-1);
	uint2 i11		=uint2(i1,j1);
	uint2 i21		=uint2(i2,j1);
	uint2 i12		=uint2(i1,j2);
	uint2 i22		=uint2(i2,j2);
	// x = right, y = up, z = left, w = down
	vec4 f11		=imageTexture[i11];
	vec4 f21		=imageTexture[i21];
	vec4 f12		=imageTexture[i12];
	vec4 f22		=imageTexture[i22];
	vec4 de11		=depthTexture[i11];
	vec4 de21		=depthTexture[i21];
	vec4 de12		=depthTexture[i12];
	vec4 de22		=depthTexture[i22];

	if(do_fallback)
	{
		f11			*=de11.z;
		f21			*=de21.z;
		f12			*=de12.z;
		f22			*=de22.z;
		f11			+=fallbackTexture[i11]*(1.0-de11.z);
		f21			+=fallbackTexture[i21]*(1.0-de21.z);
		f12			+=fallbackTexture[i12]*(1.0-de12.z);
		f22			+=fallbackTexture[i22]*(1.0-de22.z);
	}
	float d11		=depthToLinearDistance(dot(de11,depthMask),depthToLinFadeDistParams);
	float d21		=depthToLinearDistance(dot(de21,depthMask),depthToLinFadeDistParams);
	float d12		=depthToLinearDistance(dot(de12,depthMask),depthToLinFadeDistParams);
	float d22		=depthToLinearDistance(dot(de22,depthMask),depthToLinFadeDistParams);

	// But now we modify these values:
	float D1		=saturate((d-d11)/(d21-d11));
	float delta1	=abs(d21-d11);			
	f11				=lerp(f11,f21,delta1*D1);
	f21				=lerp(f21,f11,delta1*(1-D1));
	float D2		=saturate((d-d12)/(d22-d12));
	float delta2	=abs(d22-d12);			
	f12				=lerp(f12,f22,delta2*D2);
	f22				=lerp(f22,f12,delta2*(1-D2));

	vec4 f1			=lerp(f11,f21,xy.x);
	vec4 f2			=lerp(f12,f22,xy.x);
	
	float d1		=lerp(d11,d21,xy.x);
	float d2		=lerp(d12,d22,xy.x);
	
	float D			=saturate((d-d1)/(d2-d1));
	float delta		=abs(d2-d1);

	f1				=lerp(f1,f2,delta*D);
	f2				=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(f1,f2,xy.y);

	return f;
#endif
}
// Blending (not just clouds but any low-resolution alpha-blended volumetric image) into a hi-res target.
// Requires a near and a far image, a low-res depth texture with far (true) depth in the x, near depth in the y and edge in the z;
// a hi-res MSAA true depth texture.
vec4 NearFarDepthCloudBlend(vec2 texCoords
							,Texture2D lowResFarTexture
							,Texture2D lowResNearTexture
							,Texture2D hiResDepthTexture
							,Texture2D lowResDepthTexture
							,Texture2D<vec4> depthTexture
							,Texture2DMS<vec4> depthTextureMS
							,vec4 viewportToTexRegionScaleBias
							,vec4 depthToLinFadeDistParams
							,vec4 hiResToLowResTransformXYWH
							,Texture2D farInscatterTexture
							,Texture2D nearInscatterTexture
							,bool use_msaa)
{
	// texCoords.y is positive DOWNwards
	uint width,height;
	lowResNearTexture.GetDimensions(width,height);
	vec2 lowResDims	=vec2(width,height);

	vec2 depth_texc	=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	int2 hires_depth_pos2;
	int numSamples;
	if(use_msaa)
		GetMSAACoordinates(depthTextureMS,depth_texc,hires_depth_pos2,numSamples);
	else
	{
		GetCoordinates(depthTexture,depth_texc,hires_depth_pos2);
		numSamples=1;
	}
	vec2 lowResTexCoords		=hiResToLowResTransformXYWH.xy+hiResToLowResTransformXYWH.zw*texCoords;
	// First get the values that don't vary with MSAA sample:
	vec4 cloudFar;
	vec4 cloudNear				=vec4(0,0,0,1.0);
	vec4 lowres					=texture_clamp_lod(lowResDepthTexture,lowResTexCoords,0);
	vec4 hires					=texture_clamp_lod(hiResDepthTexture,texCoords,0);
	float lowres_edge			=lowres.z;
	float hires_edge			=hires.z;
	vec4 result					=vec4(0,0,0,0);
	vec2 nearFarDistLowRes		=depthToLinearDistance(lowres.yx,depthToLinFadeDistParams);
	vec4 insc					=vec4(0,0,0,0);
	vec4 insc_far				=texture_nearest_lod(farInscatterTexture,texCoords,0);
	vec4 insc_near				=texture_nearest_lod(nearInscatterTexture,texCoords,0);
	if(lowres_edge>0.0)
	{
		vec2 nearFarDistHiRes	=vec2(1.0,0.0);
		for(int i=0;i<numSamples;i++)
		{
			float hiresDepth=0.0;
			if(use_msaa)
				hiresDepth=depthTextureMS.Load(hires_depth_pos2,i).x;
			else
				hiresDepth=depthTexture[hires_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		// Find the near and far depths at full resolution.
			if(trueDist<nearFarDistHiRes.x)
				nearFarDistHiRes.x=trueDist;
			if(trueDist>nearFarDistHiRes.y)
				nearFarDistHiRes.y=trueDist;
		}
		// Given that we have the near and far depths, 
		// At an edge we will do the interpolation for each MSAA sample.
		float hiResInterp	=0.0;
		for(int j=0;j<numSamples;j++)
		{
			float hiresDepth=0.0;
			if(use_msaa)
				hiresDepth	=depthTextureMS.Load(hires_depth_pos2,j).x;
			else
				hiresDepth	=depthTexture[hires_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
			cloudNear		=depthDependentFilteredImage(lowResNearTexture	,lowResFarTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(0,1.0,0,0),depthToLinFadeDistParams,trueDist,true);
			cloudFar		=depthDependentFilteredImage(lowResFarTexture	,lowResFarTexture	,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist,false);
			float interp	=saturate((nearFarDistLowRes.y-trueDist)/abs(nearFarDistLowRes.y-nearFarDistLowRes.x));
			vec4 add		=lerp(cloudFar,cloudNear,lowres_edge*interp);
			result			+=add;
		
			if(use_msaa)
			{
				hiResInterp		=hires_edge*saturate((nearFarDistHiRes.y-trueDist)/(nearFarDistHiRes.y-nearFarDistHiRes.x));
				insc			=lerp(insc_far,insc_near,hiResInterp);
				result.rgb		+=insc.rgb*add.a;
			}
		}
		// atmospherics: we simply interpolate.
		if(use_msaa)
		{
			result				/=float(numSamples);
			hiResInterp			/=float(numSamples);
		}
		else
		{
			result.rgb		+=insc_far.rgb*result.a;
		}
		//result=vec4(1.0,0,0,1.0);
	}
	else
	{
		float hiresDepth=0.0;
		// Just use the zero MSAA sample if we're not at an edge:
		if(use_msaa)
			hiresDepth			=depthTextureMS.Load(hires_depth_pos2,0).x;
		else
			hiresDepth			=depthTexture[hires_depth_pos2].x;
		float trueDist		=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		result				=depthDependentFilteredImage(lowResFarTexture,lowResFarTexture,lowResDepthTexture,lowResDims,lowResTexCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist,false);
		result.rgb			+=insc_far.rgb*result.a;
		//result.r=1;
	}
    return result;
}

struct LookupQuad
{
	vec4 _11;
	vec4 _21;
	vec4 _12;
	vec4 _22;
};

LookupQuad GetLookupQuad(Texture2D image,vec2 texc,vec2 texDims)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	uint2 idx		=floor(texc_unit);
	int i1			=max(0,idx.x);
	int i2			=min(idx.x+1,texDims.x-1);
	int j1			=max(0,idx.y);
	int j2			=min(idx.y+1,texDims.y-1);
	uint2 i11		=uint2(i1,j1);
	uint2 i21		=uint2(i2,j1);
	uint2 i12		=uint2(i1,j2);
	uint2 i22		=uint2(i2,j2);

	LookupQuad q;
	q._11=image[i11];
	q._21=image[i21];
	q._12=image[i12];
	q._22=image[i22];
	return q;
}
LookupQuad GetDepthLookupQuad(Texture2D image,vec2 texc,vec2 texDims,vec4 depthToLinFadeDistParams)
{
	LookupQuad q=GetLookupQuad(image,texc,texDims);
	q._11.xy		=depthToLinearDistance(q._11.xy,depthToLinFadeDistParams);
	q._21.xy		=depthToLinearDistance(q._21.xy,depthToLinFadeDistParams);
	q._12.xy		=depthToLinearDistance(q._12.xy,depthToLinFadeDistParams);
	q._22.xy		=depthToLinearDistance(q._22.xy,depthToLinFadeDistParams);
	return q;
}
vec4 depthFilteredTexture(	LookupQuad image
							,LookupQuad fallback
							,LookupQuad depth
							,vec2 texDims
							,vec2 texc
							,vec4 depthMask
							,vec4 depthToLinFadeDistParams
							,float d
							,bool do_fallback)
{
	vec2 texc_unit	=texc*texDims-vec2(.5,.5);
	vec2 xy			=frac(texc_unit);
	// x = right, y = up, z = left, w = down
	vec4 f11		=image._11;
	vec4 f21		=image._21;
	vec4 f12		=image._12;
	vec4 f22		=image._22;

	if(do_fallback)
	{
		f11			*=depth._11.z;
		f21			*=depth._21.z;
		f12			*=depth._12.z;
		f22			*=depth._22.z;
		f11			+=fallback._11*(1.0-depth._11.z);
		f21			+=fallback._21*(1.0-depth._21.z);
		f12			+=fallback._12*(1.0-depth._12.z);
		f22			+=fallback._22*(1.0-depth._22.z);
	}
	float d11		=depth._11;
	float d21		=depth._21;
	float d12		=depth._12;
	float d22		=depth._22;

	// But now we modify these values:
	float D1		=saturate((d-d11)/(d21-d11));
	float delta1	=abs(d21-d11);			
	f11				=lerp(f11,f21,delta1*D1);
	f21				=lerp(f21,f11,delta1*(1-D1));
	float D2		=saturate((d-d12)/(d22-d12));
	float delta2	=abs(d22-d12);			
	f12				=lerp(f12,f22,delta2*D2);
	f22				=lerp(f22,f12,delta2*(1-D2));

	vec4 f1			=lerp(f11,f21,xy.x);
	vec4 f2			=lerp(f12,f22,xy.x);
	
	float d1		=lerp(d11,d21,xy.x);
	float d2		=lerp(d12,d22,xy.x);
	
	float D			=saturate((d-d1)/(d2-d1));
	float delta		=abs(d2-d1);

	f1				=lerp(f1,f2,delta*D);
	f2				=lerp(f2,f1,delta*(1.0-D));

	vec4 f			=lerp(f1,f2,xy.y);

	return f;
}

vec4 Composite(vec2 texCoords
				,Texture2D lowResFarTexture
				,Texture2D lowResNearTexture
				,Texture2D hiResDepthTexture
				,uint2 hiResDims
				,Texture2D lowResDepthTexture
				,uint2 lowResDims
				,Texture2D<vec4> depthTexture
				,Texture2DMS<vec4> depthTextureMS
				,uint2 fullResDims
				,vec4 viewportToTexRegionScaleBias
				,vec4 depthToLinFadeDistParams
				,vec4 fullResToLowResTransformXYWH
				,vec4 fullResToHighResTransformXYWH
				,Texture2D farInscatterTexture
				,Texture2D nearInscatterTexture
				,bool use_msaa)
{
	// texCoords.y is positive DOWNwards
	
	vec2 depth_texc			=viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias);
	int2 hires_depth_pos2;
	int numSamples;
	if(use_msaa)
		GetMSAACoordinates(depthTextureMS,depth_texc,hires_depth_pos2,numSamples);
	else
	{
		GetCoordinates(depthTexture,depth_texc,hires_depth_pos2);
		numSamples=1;
	}
	vec2 lowresTexel			=vec2(1.0/lowResDims.x,1.0/lowResDims.y);
	vec2 hiresTexel				=vec2(1.0/hiResDims.x,1.0/hiResDims.y);
	vec2 fullresTexel			=vec2(1.0/fullResDims.x,1.0/fullResDims.y);
	vec2 lowResTexCoords		=fullResToLowResTransformXYWH.xy+texCoords*fullResToLowResTransformXYWH.zw;
	vec2 hiResTexCoords			=fullResToHighResTransformXYWH.xy+texCoords*fullResToHighResTransformXYWH.zw;
	
	//hiResTexCoords				-=0.5*fullresTexel;

	vec4 lowres_depths			=texture_nearest_lod(lowResDepthTexture	,lowResTexCoords	,0);
	vec4 hires_depths			=texture_nearest_lod(hiResDepthTexture	,hiResTexCoords		,0);
	float lowres_edge			=lowres_depths.z;
	float hires_edge			=hires_depths.z;
	vec4 result					=vec4(0,0,0,0);
	vec4 insc					=vec4(0,0,0,0);
	if(lowres_edge>0.0||hires_edge>0.0)
	{
		vec4 cloudNear				=texture_nearest_lod(lowResNearTexture	,lowResTexCoords	,0);
		vec4 cloudFar				=texture_nearest_lod(lowResFarTexture		,lowResTexCoords	,0);
		vec4 insc_far				=texture_nearest_lod(farInscatterTexture	,hiResTexCoords		,0);
		vec4 insc_near				=texture_nearest_lod(nearInscatterTexture	,hiResTexCoords		,0);
		vec2 nearFarDistLowRes		=depthToLinearDistance(lowres_depths.yx	,depthToLinFadeDistParams);
		vec2 nearFarDistHiRes		=depthToLinearDistance(hires_depths.yx	,depthToLinFadeDistParams);
		// Given that we have the near and far depths, 
		// At an edge we will do the interpolation for each MSAA sample.
		float hiResInterp	=0.0;
		for(int j=0;j<numSamples;j++)
		{
			float hiresDepth=0.0;
			if(use_msaa)
				hiresDepth	=depthTextureMS.Load(hires_depth_pos2,j).x;
			else
				hiresDepth	=depthTexture[hires_depth_pos2].x;
			float trueDist	=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
			float interp	=saturate((nearFarDistLowRes.y-trueDist)/abs(nearFarDistLowRes.y-nearFarDistLowRes.x));
			vec4 add		=lerp(cloudFar,cloudNear,interp);
			result			+=add;
			if(hires_edge>0.0)
			{
				hiResInterp		=saturate((nearFarDistHiRes.y-trueDist)/(nearFarDistHiRes.y-nearFarDistHiRes.x));
				insc			=lerp(insc_far,insc_near,hiResInterp);
				result.rgb		+=insc.rgb*add.a;
			//	result.r=trueDist;
			}
		}
		// atmospherics: we simply interpolate.
		if(use_msaa)
		{
			result				/=float(numSamples);
		}
		//result.gb=nearFarDistHiRes.xy;
	}
	if(lowres_edge<=0.0||hires_edge<=0.0)
	{
		float hiresDepth=0.0;
		// Just use the zero MSAA sample if we're not at an edge:
		if(use_msaa)
			hiresDepth			=depthTextureMS.Load(hires_depth_pos2,0).x;
		else
			hiresDepth			=depthTexture[hires_depth_pos2].x;
		float trueDist			=depthToLinearDistance(hiresDepth,depthToLinFadeDistParams);
		if(lowres_edge<=0.0)
			result				+=texture_clamp_lod(lowResFarTexture,lowResTexCoords,0);
		if(hires_edge<=0.0)
		{	
			vec4 insc_far		=texture_clamp_lod(farInscatterTexture,hiResTexCoords,0);
			result.rgb			+=insc_far.rgb*result.a;
			//result.r=1;
		}
	}
//	if(texCoords.y>0.5)
//		result.b=1.0;
    return result;
}
#endif
#endif
