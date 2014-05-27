#ifndef GPU_SKY_CONSTANTS_SL
#define GPU_SKY_CONSTANTS_SL

uniform_buffer GpuSkyConstants SIMUL_BUFFER_REGISTER(8)
{
	uniform vec2 texSize;
	uniform vec2 tableSize;
	
	uniform uint3 threadOffset;
	uniform float emissivity;

	uniform vec3 directionToMoon;
	uniform float distanceKm;

	uniform float texelOffset;
	uniform float prevDistanceKm;

	uniform float maxOutputAltKm;
	uniform float planetRadiusKm;

	uniform float maxDensityAltKm;
	uniform float hazeBaseHeightKm;
	uniform float hazeScaleHeightKm;
	uniform float seaLevelTemperatureK;

	uniform vec3 rayleigh;
	uniform float XovercastBaseKmX;
	uniform vec3 hazeMie;
	uniform float XovercastRangeKmX;
	uniform vec3 ozone;
	uniform float overcastX;

	uniform vec3 sunIrradiance;
	uniform float maxDistanceKm;

	uniform vec3 lightDir;
	uniform float hazeEccentricity;

	uniform vec3 starlight;
	uniform float previousZCoord;

	uniform vec3 mieRayleighRatio;
	uniform float ejsyr;

	uniform vec4 yRange;

	uniform float texCoordZ;
	uniform float AHERaH,ASJET,AETJAETJ;

};

#ifndef __cplusplus

#define pi (3.1415926536)

uint3 LinearThreadToPos2D(uint linear_pos,uint3 dims)
{
	uint Y				=linear_pos/dims.x;
	uint X				=linear_pos-Y*dims.x;
	uint3 pos			=uint3(X,Y,0);
	return pos;
}

float getHazeFactorAtAltitude(float alt_km)
{
	if(alt_km<hazeBaseHeightKm)
		alt_km=hazeBaseHeightKm;
	float val=exp((hazeBaseHeightKm-alt_km)/hazeScaleHeightKm);
	return val;
}

float getHazeOpticalLength(float sine_elevation,float h_km)
{
	float R=planetRadiusKm;
	float Rh=R+h_km;
	float RH=R+hazeBaseHeightKm;
	float c=sqrt(1.0-sine_elevation*sine_elevation);
	float u=RH*RH-Rh*Rh*c*c;
	float U=R*R-Rh*Rh*c*c;
	// If the ray passes through the earth, infinite optical length.
	if(sine_elevation<0&&U>0.0)
		return 1000000.0;
	float haze_opt_len=0.0;
	// If we have a solution, there exists a path through the constant haze area.
	if(sine_elevation<0&&u>0)
		haze_opt_len=2.0*sqrt(u);
	// Next, the inward path, if that exists.
	float Rmin=Rh*c;
	if(sine_elevation>0.0)
		Rmin=Rh;
	// But if the inward path goes into the constant area, include only the part outside that area.
	if(sine_elevation<0.0&&u>0.0)
		Rmin=RH;
	float h1=Rh-RH;
	float h2=Rmin-RH;
	float s=sine_elevation;
	float n=hazeScaleHeightKm;
	// s<0, and h2<h1
	if(s<0.0)
		haze_opt_len+=n/s*(exp(-h1/n)-exp(-h2/n));
	// Now the outward path, in this case h2 -> infinity
	// and elevation is reversed.
	if(s<0.0)
		s*=-1.0;
	if(s<0.01)
		s=0.01;
	haze_opt_len+=n/s*(exp(-abs(h2)/n));
	return haze_opt_len;
}

vec4 getSunlightFactor(Texture2D optical_depth_texture,float alt_km,vec3 DirectionToLight)
{
	float sine				=clamp(DirectionToLight.z,-1.0,1.0);
	vec2 table_texc			=vec2(tableSize.x*(0.5+0.5*sine),tableSize.y*(alt_km/maxDensityAltKm));

	table_texc				+=vec2(texelOffset,texelOffset);
	table_texc				=vec2(table_texc.x/tableSize.x,table_texc.y/tableSize.y);
	
	vec4 lookup				=texture_clamp_lod(optical_depth_texture,table_texc,0);
	float illuminated_length=lookup.x;
	float vis				=lookup.y;
	float ozone_length		=lookup.w;
	float haze_opt_len		=getHazeOpticalLength(sine,alt_km);
	vec4 factor				=vec4(vis,vis,vis,vis);
	factor.rgb				*=exp(-rayleigh*illuminated_length-hazeMie*haze_opt_len-ozone*ozone_length);
	return factor;
}

vec4 getSunlightFactor2(Texture2D optical_depth_texture,float alt_km,vec3 DirectionToLight)
{
	float sine				=clamp(DirectionToLight.z,-1.0,1.0);
	vec2 table_texc			=vec2(0.5+0.5*sine,alt_km/maxDensityAltKm);

	table_texc				+=vec2(texelOffset/tableSize.x,texelOffset/tableSize.y);
	
	vec4 lookup				=texture_clamp_lod(optical_depth_texture,table_texc,0);
	float illuminated_length=lookup.x;
	float vis				=lookup.y;
	float ozone_length		=lookup.w;
	float haze_opt_len		=getHazeOpticalLength(sine,alt_km);
	vec4 factor				=vec4(vis,vis,vis,vis);
	factor.rgb				*=exp(-rayleigh*illuminated_length-hazeMie*haze_opt_len-ozone*ozone_length);

	return factor;
}

float getShortestDistanceToAltitude(float sine_elevation,float start_h_km,float finish_h_km)
{
	float RH		=planetRadiusKm+finish_h_km;
	float Rh		=planetRadiusKm+start_h_km;
	float cosine	=-sine_elevation;
	float b			=-2.0*Rh*cosine;
	float c			=Rh*Rh-RH*RH;
	float b24c		=b*b-4*c;
	if(b24c<0)
		return -1.0;
	float dist;
	float s=sqrt(b24c);
	//if(b+s>=0.0)
		dist=0.5*(-b+s);
	//else
	//	dist=0.5*(-b-s);
	return dist;
}

float getDistanceToSpace(float sine_elevation,float h_km)
{
	return getShortestDistanceToAltitude(sine_elevation,h_km,maxDensityAltKm);
}

vec4 Insc(Texture2D input_texture,Texture3D loss_texture,Texture2D density_texture,Texture2D optical_depth_texture,vec2 texCoords)
{
	vec4 previous_insc	=texture_nearest_lod(input_texture,texCoords.xy,0);
	vec3 previous_loss	=texture_nearest_lod(loss_texture,vec3(texCoords.xy,pow(distanceKm/maxDistanceKm,0.5)),0).rgb;// should adjust texCoords - we want the PREVIOUS loss!
	float sin_e			=clamp(1.0-2.0*(texCoords.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texCoords.x*texSize.x-texelOffset)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture_clamp_lod(density_texture,vec2(dens_texc,0.5),0);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(sunIrradiance,1.0)*getSunlightFactor(optical_depth_texture,alt_km,lightDir);
	light.rgb			*=RAYLEIGH_BETA_FACTOR;
	vec4 insc			=light;
	//insc				*=1.0-getOvercastAtAltitudeRange(alt_1_km,alt_2_km);
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	insc.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-insc.w*stepLengthKm*haze_factor*hazeMie.x);
	insc.rgb			*=previous_loss.rgb;
	insc.rgb			+=previous_insc.rgb;

	insc.w				=saturate((1.0-mie_factor)/(1.0-total_ext.x+0.0001));
	float lossw=1.0;
	insc.w				=(lossw)*(1.0-previous_insc.w)*insc.w+previous_insc.w;
//insc.rgb=loss.rgb;//RAYLEIGH_BETA_FACTOR*insc.rgb;
    return			insc;
}

// What spectral radiance is added on a light path towards the viewer, due to illumination of
// a volume of air by the surrounding sky?
// in cpp:
//	float cos0=dir_to_sun.z;
//	Skylight=GetAnisotropicInscatterFactor(true,hh,pif/2.f,0,1e5f,dir_to_sun,dir_to_moon,haze,false,1);
//	Skylight*=GetInscatterAngularMultiplier(cos0,Skylight.w,hh);

vec3 getSkylight(float alt_km, Texture3D insc_texture)
{
// The inscatter factor, at this altitude looking straight up, is given by:
	vec4 insc		=texture_clamp_lod(insc_texture,vec3(sqrt(alt_km/maxOutputAltKm),0.0,1.0),0);
	vec3 skylight	=InscatterFunction(insc,hazeEccentricity,0.0,mieRayleighRatio);
	return skylight;
}

vec3 Blackbody(Texture2D blackbody_texture,float T_K)
{
    float tc    =saturate((T_K-200.0)/200.0);
    tc          =saturate(tc+texelOffset/tableSize.x);
	vec3 bb		=texture_clamp_lod(blackbody_texture,vec2(tc,tc),0).rgb;
    return bb;
}

vec4 Skyl(Texture3D insc_texture
		,Texture2D density_texture
		,Texture2D blackbody_texture
		,vec3 previous_loss
		,vec4 previous_skyl
		,float maxOutputAltKm
		,float maxDistanceKm
		,float maxDensityAltKm
		,float spaceDistKm
		,float viewAltKm
		,float dist_km
		,float prevDist_km
		,float sin_e
		,float cos_e)
{
	float maxd			=min(spaceDistKm,dist_km);
	float mind			=min(spaceDistKm,prevDist_km);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture_clamp_lod(density_texture,vec2(dens_texc,0.5),0);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(starlight+getSkylight(alt_km,insc_texture),0.0);
	vec4 skyl			=light;
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	skyl.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-skyl.w*stepLengthKm*haze_factor*hazeMie.x);
	skyl.w				=saturate((1.0-mie_factor)/(1.0-total_ext.x+0.0001));
#if 1//def BLACKBODY
	float dens_dist	=dens_factor*stepLengthKm;
	float emis_ext  =exp(-emissivity*dens_dist);
	vec3 bb;
	float T         =seaLevelTemperatureK*lookups.w;
	bb.xyz          =Blackbody(blackbody_texture,T);

	skyl            *=emis_ext;
	bb              *=1.0-emis_ext;
	skyl.rgb        +=bb;
	//skyl.rgb        =0.000001*skyl.rgb+Blackbody(T);
 #endif
	//skyl.w			=(loss.w)*(1.0-previous_skyl.w)*skyl.w+previous_skyl.w;
	skyl.rgb			*=previous_loss.rgb;
	skyl.rgb			+=previous_skyl.rgb;
		
	float lossw=1.0;
	skyl.w				=(lossw)*(1.0-previous_skyl.w)*skyl.w+previous_skyl.w;
	return skyl;
}
#ifndef GLSL


void CSLoss(RWTexture3D<float4> targetTexture,Texture2D density_texture,uint3 pos,float maxOutputAltKm,float maxDistanceKm,float maxDensityAltKm)
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	vec2 texc			=(pos.xy+vec2(0.5,0.5))/vec2(dims.xy);
	vec4 previous_loss	=vec4(1.0,1.0,1.0,1.0);//texture_clamp(input_loss_texture,texc.xy);
	float sin_e			=max(-1.0,min(1.0,1.0-2.0*(texc.y*texSize.y-texelOffset)/(texSize.y-1.0)));
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texc.x*texSize.x-texelOffset)/max(texSize.x-1.0,1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float prevDist_km	=0.0;
	for(uint i=0;i<dims.z;i++)
	{
		uint3 idx			=uint3(pos.xy,i);
		float zPosition		=pow((float)(i)/((float)dims.z-1.0),2.0);
		float dist_km		=zPosition*maxDistanceKm;
		float maxd			=min(spaceDistKm,dist_km);
		float mind			=min(spaceDistKm,prevDist_km);
		float dist			=0.5*(mind+maxd);
		float stepLengthKm	=max(0.0,maxd-mind);
		float y				=planetRadiusKm+viewAltKm+dist*sin_e;
		float x				=dist*cos_e;
		float r				=sqrt(x*x+y*y);
		float alt_km		=r-planetRadiusKm;
		// lookups is: dens_factor,ozone_factor,haze_factor;
		float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
		vec4 lookups		=texture_clamp_lod(density_texture,dens_texc,0);
		float dens_factor	=lookups.x;
		float ozone_factor	=lookups.y;
		float haze_factor	=getHazeFactorAtAltitude(alt_km);
		vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie+ozone*ozone_factor;
		vec4 loss;
		loss.rgb			=exp(-extinction*stepLengthKm);
		loss.a				=(loss.r+loss.g+loss.b)/3.0;
		loss				*=previous_loss;
		targetTexture[idx]	=vec4(loss.rgb,1.0);
		prevDist_km			=dist_km;
		previous_loss		=loss;
	}
}
void CSSkyl(RWTexture3D<float4> targetTexture,Texture3D loss_texture,Texture3D insc_texture
	,Texture2D density_texture,Texture2D blackbody_texture,uint3 pos,float maxOutputAltKm,float maxDistanceKm,float maxDensityAltKm)
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	vec2 texc			=(pos.xy+vec2(0.5,0.5))/vec2(dims.xy);
	
	vec4 previous_skyl	=vec4(0.0,0.0,0.0,1.0);
	float sin_e			=max(-1.0,min(1.0,1.0-2.0*(texc.y*texSize.y-texelOffset)/(texSize.y-1.0)));
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texc.x*texSize.x-texelOffset)/max(texSize.x-1.0,1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);

	float prevDist_km	=0.0;
	// The midpoint of the step represented by this layer
	for(int i=0;i<int(dims.z);i++)
	{
		uint3 idx			=uint3(pos.xy,i);
		float zPosition		=pow((float)(i)/((float)dims.z-1.0),2.0);
		vec3 previous_loss	=loss_texture[idx].rgb;//vec3(IN.texc.xy,pow(distanceKm/maxDistanceKm,0.5))).rgb;// should adjust texc - we want the PREVIOUS loss!
		float dist_km		=zPosition*maxDistanceKm;
		if(i==dims.z-1)
			dist_km=1000.0;
		vec4 skyl	=Skyl(insc_texture
									,density_texture
									,blackbody_texture
									,previous_loss
									,previous_skyl
									,maxOutputAltKm
									,maxDistanceKm
									,maxDensityAltKm
									,spaceDistKm
									,viewAltKm
									,dist_km
									,prevDist_km
									,sin_e
									,cos_e);
#if 0
 #endif
		targetTexture[idx]	=skyl;
		prevDist_km			=dist_km;
		previous_skyl		=skyl;
	}
}

void MakeLightTable(RWTexture3D<float4> targetTexture, Texture3D insc_texture, uint3 sub_pos)
{
	// threadOffset.y determines the cycled index.
	uint3 pos			=sub_pos+threadOffset;
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	float alt_texc			=float(pos.x)/float(dims.x);
	float alt_km			=maxOutputAltKm*alt_texc;
	vec4 sunlight			=vec4(sunIrradiance,1.0)*getSunlightFactor2(optical_depth_texture,alt_km,lightDir);
	float moon_angular_radius=pi/180.0/2.0;
	float moon_angular_area_ratio=pi*moon_angular_radius*moon_angular_radius/(4.0*pi);
	vec4 moonlight			=vec4(sunIrradiance,1.0)*getSunlightFactor2(optical_depth_texture,alt_km,directionToMoon)*0.136*moon_angular_area_ratio;
	// equivalent to GetAnisotropicInscatterFactor(true,altitude_km,pi/2.f,0,1e5f,sun_irradiance,starlight,dir_to_sun,dir_to_moon,haze,overcast,false,0):
	vec4 ambientLight		=vec4(getSkylight(alt_km, insc_texture),1.0);
	uint3 pos_sun			=uint3(pos.xy,0);
    targetTexture[pos_sun]	=sunlight;
	uint3 pos_moon			=uint3(pos.xy,1);
    targetTexture[pos_moon]	=moonlight;
	uint3 pos_amb			=uint3(pos.xy,2);
    targetTexture[pos_amb]	=ambientLight;

	// Combined sun and moonlight:
	uint3 pos_both		=uint3(pos.xy,3);
    targetTexture[pos_both]	=sunlight+moonlight;
}
#endif
#endif

#endif