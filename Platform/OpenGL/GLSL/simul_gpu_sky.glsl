uniform sampler2D optical_depth_texture;

uniform float distKm;
uniform float prevDistKm;
uniform float maxOutputAltKm;
uniform float planetRadiusKm;
uniform float maxDensityAltKm;
uniform float hazeBaseHeightKm;
uniform float hazeScaleHeightKm;

uniform float overcastBaseKm;
uniform float overcastRangeKm;
uniform float overcast;

uniform vec3 rayleigh;
uniform vec3 hazeMie;
uniform vec3 ozone;
uniform vec2 texSize;
uniform vec2 tableSize;

#define pi (3.1415926536)

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

vec4 getSunlightFactor(float alt_km,vec3 DirectionToLight)
{
	float sine=DirectionToLight.z;
	vec2 table_texc=vec2(0.5+0.5*sine,alt_km/maxDensityAltKm);
	table_texc*=tableSize;//vec2(tableSize.x-1.0,tableSize.y-1.0);
	table_texc+=vec2(0.5,0.5);
	table_texc/=tableSize;
	vec4 lookup=texture(optical_depth_texture,table_texc);
	float illuminated_length=lookup.x;
	float vis=lookup.y;
	float ozone_length=lookup.w;
	float haze_opt_len=getHazeOpticalLength(sine,alt_km);
	vec4 factor=vec4(vis,vis,vis,vis);
	factor.rgb*=exp(-rayleigh*illuminated_length-hazeMie*haze_opt_len-ozone*ozone_length);
	return factor;
}

float getShortestDistanceToAltitude(float sine_elevation,float start_h_km,float finish_h_km)
{
	float RH=planetRadiusKm+finish_h_km;
	float Rh=planetRadiusKm+start_h_km;
	float cosine=-sine_elevation;
	float b=-2.0*Rh*cosine;
	float c=Rh*Rh-RH*RH;
	float b24c=b*b-4*c;
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


float getOvercastAtAltitude(float h_km)
{
	if(h_km<overcastBaseKm)
		return overcast;
	if(h_km>overcastBaseKm+overcastRangeKm)
		return 0.f;
	return overcast*(h_km-overcastBaseKm)/overcastRangeKm;
}

float getOvercastAtAltitudeRange(float alt1_km,float alt2_km)
{
	if(alt1_km>=overcastBaseKm+overcastRangeKm)
		return 0.0;
	if(alt2_km<=alt1_km)
	{
		float temp=alt1_km;
		alt1_km=alt2_km;
		alt2_km=temp;
	}
	if(alt2_km<=overcastBaseKm)
		return overcast;
	if(alt1_km==alt2_km)
		return getOvercastAtAltitude(alt1_km);
	float diff_km=alt2_km-alt1_km;
	// 4 cases:
	if(alt1_km<overcastBaseKm)
	{
	//		1 - alt1<base, alt2 in between:
		if(alt2_km<overcastBaseKm+overcastRangeKm)
		{
			float proportion=(overcastBaseKm-alt1_km)/diff_km;
			return proportion*overcast+(1.0-proportion)*getOvercastAtAltitude(0.5*(overcastBaseKm+alt2_km));
		}
	//		1 - alt1<base, alt2>highest:
		else
		{
			float proportion1=(overcastBaseKm-alt1_km)/diff_km;
			float proportion2=overcastRangeKm/diff_km;
			return proportion1*overcast+proportion2*getOvercastAtAltitude(overcastBaseKm+0.5*overcastRangeKm);
		}
	}
	//		3 - alt1 in between, alt2 in between
	else if(alt2_km<overcastBaseKm+overcastRangeKm)
	{
		return getOvercastAtAltitude(0.5*(alt1_km+alt2_km));
	}
	//		4 - alt1 in between, alt2 above.
	else
	{
		float proportion=(alt2_km-(overcastBaseKm+overcastRangeKm))/diff_km;
		return (1.0-proportion)*getOvercastAtAltitude(0.5*(alt1_km+overcastBaseKm+overcastRangeKm));
	}
}