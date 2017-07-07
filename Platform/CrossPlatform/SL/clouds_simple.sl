//  Copyright (c) 2007-2016 Simul Software Ltd. All rights reserved.
#ifndef CLOUDS_SIMPLE_SL
#define CLOUDS_SIMPLE_SL

RaytracePixelOutput RaytraceCloudsStatic(Texture3D cloudDensity
											,Texture3D cloudLight
											,Texture2D rainMapTexture
											,Texture3D noiseTexture3D
											,Texture2D lightTableTexture
											,Texture2D illuminationTexture
											,Texture2D rainbowLookupTexture
											,Texture2D coronaLookupTexture
											,Texture2D lossTexture
											,Texture2D inscTexture
											,Texture2D skylTexture
											,Texture3D inscatterVolumeTexture
                                            ,bool do_depth_mix
											,vec4 dlookup
											,vec3 view
											,vec4 clip_pos
											,vec3 volumeTexCoordsXyC
											,bool noise
											,bool do_rain_effect
											,bool do_rainbow
											,vec3 cloudIrRadiance1
											,vec3 cloudIrRadiance2
											,int numSteps
											,const int num_interp)
{
	RaytracePixelOutput res;
	for(int ii=0;ii<num_interp;ii++)
		res.colour[ii]			=vec4(0,0,0,1.0);
	res.nearFarDepth		=dlookup;

	float s					=saturate((directionToSun.z+MIN_SUN_ELEV)/0.01);
	vec3 lightDirection		=lerp(directionToMoon,directionToSun,s);

	float cos0				=dot(lightDirection.xyz,view.xyz);
	float sine				=view.z;

	float min_z				=cornerPosKm.z-(fractalScale.z*1.5)/inverseScalesKm.z;
	float max_z				=cornerPosKm.z+(1.0+fractalScale.z*1.5)/inverseScalesKm.z;
	if(do_rain_effect)
		min_z				=-1.0;

	else if(view.z<-0.01&&viewPosKm.z<cornerPosKm.z-fractalScale.z/inverseScalesKm.z)
		return res;
	float solidDist_nearFar	[NUM_CLOUD_INTERP];
	vec2 nfd				=(dlookup.yx)+100.0*step(vec2(1.0,1.0), dlookup.yx);

	float n							=nfd.x;
	float f							=nfd.y;
	solidDist_nearFar[0]			=n;
	solidDist_nearFar[num_interp-1]	=f;
	for(int l=1;l<num_interp-1;l++)
	{
		float interp			=float(l)/float(num_interp-1);
		solidDist_nearFar[l]	=lerp(n,f,interp);
	}
	vec2 fade_texc			=vec2(0.0,0.5*(1.0-sine));
	// Lookup in the illumination texture.
	vec2 illum_texc			=vec2(atan2(view.x,view.y)/(3.1415926536*2.0),fade_texc.y);
	vec4 illum_lookup		=texture_wrap_mirror_lod(illuminationTexture,illum_texc,0);
	// TODO: reimplement illum if needed.
	vec2 nearFarTexc		=vec2(0,1.0);	//illum_lookup.xy;
	float meanFadeDistance	=1.0;
	float minDistance		=1.0;
	float maxDistance		=0.0;
	// Precalculate hg effects
	float BetaClouds		=lightResponse.x*HenyeyGreenstein(cloudEccentricity,cos0);
	float BetaRayleigh		=CalcRayleighBeta(cos0);
	float BetaMie			=HenyeyGreenstein(hazeEccentricity,cos0);

	vec4 rainbowColour;
	if(do_rainbow)
		rainbowColour=RainbowAndCorona(rainbowLookupTexture,coronaLookupTexture,dropletRadius,
												rainbowIntensity,view,lightDirection);
	float moisture			=0.0;

	vec3 world_pos			=viewPosKm;
	view.xyz+=vec3(0.00001,0.00001,0.00001);
	// This provides the range of texcoords that is lit.
	// In c_offset, we want 1's or -1's. NEVER zeroes!
	int3 c_offset			=int3(2.0*step(vec3(0,0,0),view.xyz)-vec3(1,1,1));
	int3 start_c_offset		=-c_offset;
	start_c_offset			=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled			=view/scaleOfGridCoords;
	viewScaled				=normalize(viewScaled);

	vec3 offset_vec			=vec3(0,0,0);
	
	{
		float a		=1.0/(saturate(view.z)+0.00001);
		offset_vec	+=max(0.0,min_z-world_pos.z)*vec3(view.x*a,view.y*a,1.0);
	}
	
	{
		float a		=1.0/(saturate(-view.z)+0.00001);
		offset_vec	+=max(0.0,world_pos.z-max_z)*vec3(view.x*a,view.y*a,-1.0);
	}
	vec3 halfway					=0.5*(lightDirection-view);
	world_pos						+=offset_vec;
	
	float distanceKm				=length(offset_vec);

	float distScale					=0.6/maxFadeDistanceKm;
	float K							=log(maxCloudDistanceKm);
	bool found=false;
	float stepKm = K*(1.2 + distanceKm) / float(numSteps);
	/// (1.0 + 100.0*abs(view.z));
	
	vec3 amb_dir=view;
	for(int i=0;i<768;i++)
	{
		//world_pos					+=0.001*view;
		if((view.z<0&&world_pos.z<min_z)||(view.z>0&&world_pos.z>max_z)||distanceKm>maxCloudDistanceKm)//||solidDist_nearFar.y<lastFadeDistance)
			break;
		stepKm						*=(1.0+K/float(numSteps));
		distanceKm					+=stepKm;
		// We fade out the intermediate steps as we approach the boundary of a detail change.
		// Now sample at the end point:
		world_pos					+=stepKm*view;
		vec3 cloudWorldOffsetKm		=world_pos-cornerPosKm;
		vec3 cloudTexCoords			=(cloudWorldOffsetKm)*inverseScalesKm;
		float fade					=1.0;
		float fadeDistance			=saturate(distanceKm/maxFadeDistanceKm);

		// maxDistance is the furthest we can *see*.
		maxDistance					=max(fadeDistance,maxDistance);
		
		if(fade>0)
		{
			vec4 density =  sample_3d_lod(cloudDensity, cloudSamplerState, cloudTexCoords, 0);
	
			//if(!found)
			{
			//	found = found || (density.z > 0);
			}
			//if(found)
			{
				vec3 noise_texc			=(world_pos.xyz)*noise3DTexcoordScale+noise3DTexcoordOffset;

				vec4 noiseval			=vec4(0,0,0,0);
				if(noise&&12.0*fadeDistance<4.0)
					noiseval			=density.x*texture_3d_wrap_lod(noiseTexture3D,noise_texc,12.0*fadeDistance);
				vec4 light				=vec4(1,1,1,1);
				calcDensity(cloudDensity,cloudLight,cloudTexCoords,fade,noiseval,fractalScale,fadeDistance,density,light);
				if(do_rain_effect)
				{
					// The rain fall angle is used:
					float dm			=rainEffect*fade*GetRainAtOffsetKm(rainMapTexture,cloudWorldOffsetKm,inverseScalesKm, world_pos, rainCentreKm.xy, rainRadiusKm,rainEdgeKm);
					moisture			+=0.01*dm*light.x;
					density.z			=saturate(density.z+dm);
				}
				if(density.z>0)
				{
					if(noise)
					{
						vec3 worley_texc	=(world_pos.xyz+worleyTexcoordOffset)*worleyTexcoordScale;
						minDistance			=min(max(0,fadeDistance-density.z*stepKm/maxFadeDistanceKm), minDistance);
						vec4 worley			=texture_wrap_lod(smallWorleyTexture3D,worley_texc,0);
						float wo			=4*density.y*(worley.w-0.6)*saturate(1.0/(12.0*fadeDistance));//(worley.x+worley.y+worley.z+worley.w-0.6*(1.0+0.5+0.25+0.125));
						density.z			=saturate(0.3+(1.0+alphaSharpness)*((density.z+wo)-0.3-saturate(0.6-density.z)));
						amb_dir				=lerp(amb_dir,worley.xyz,0.1*density.z);
					}
					//density.xy		*=1.0+wo;
					float brightness_factor;
					fade_texc.x				=sqrt(fadeDistance);
					vec3 volumeTexCoords	=vec3(volumeTexCoordsXyC.xy,fade_texc.x);

					ColourStep(res.colour, meanFadeDistance, brightness_factor
								,lossTexture, inscTexture, skylTexture, inscatterVolumeTexture, lightTableTexture
								,density, light, distanceKm, fadeDistance
								,world_pos
								,cloudTexCoords, fade_texc, nearFarTexc
								,1.0, volumeTexCoords, amb_dir
								,BetaClouds, BetaRayleigh, BetaMie
								,solidDist_nearFar, noise, do_depth_mix, distScale,0, noiseval);
					if(res.colour[0].a*brightness_factor<0.003)
					{
						for(int o=0;o<num_interp;o++)
							res.colour[o].a = 0.0;
						break;
					}
				}
			}
		}
	}
#ifndef INFRARED
	if(do_rainbow)
		res.colour[num_interp-1].rgb		+=saturate(moisture)*sunlightColour1.rgb/25.0*rainbowColour.rgb;
#endif
	//res.nearFarDepth.y	=	max(0.00001,res.nearFarDepth.x-res.nearFarDepth.y);
	//res.nearFarDepth.z	=	max(0.0000001,res.nearFarDepth.x-meanFadeDistance);// / maxFadeDistanceKm;// min(res.nearFarDepth.y, max(res.nearFarDepth.x + distScale, minDistance));// min(distScale, minDistance);
	res.nearFarDepth.w	=	meanFadeDistance;
//for(int i=0;i<num_interp;i++)
	return res;
}
#endif
