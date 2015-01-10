#ifndef GPU_SKY_CONSTANTS_SL
#define GPU_SKY_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(GpuSkyConstants,8)
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

	uniform int3 targetSize;
	uniform float fuyofyu;
SIMUL_CONSTANT_BUFFER_END
#endif