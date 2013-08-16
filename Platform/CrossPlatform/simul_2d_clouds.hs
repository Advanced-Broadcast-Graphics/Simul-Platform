#ifndef SIMUL_2D_CLOUDS_HS
#define SIMUL_2D_CLOUDS_HS

uniform_buffer Cloud2DConstants R11
{
	uniform vec4 viewportToTexRegionScaleBias;
	uniform mat4 worldViewProj;
	uniform vec3 origin;
	uniform float globalScale;

	uniform vec4 lightResponse;
	uniform vec3 lightDir;
	uniform float cloudEccentricity;
	uniform vec3 eyePosition;
	uniform float maxFadeDistanceMetres;
	uniform vec3 sunlight;
	uniform float cloudInterp;
	uniform vec3 mieRayleighRatio;
	uniform float hazeEccentricity;
	uniform float detailScale;
	uniform float planetRadius,b,c;
	uniform float nearZ;
	uniform float farZ;
	uniform vec2 tanHalfFov;
	uniform vec3 depthToLinFadeDistParams;
	uniform float exposure;
};

uniform_buffer Detail2DConstants R12
{
	uniform float persistence;
	uniform float aa;
	uniform float bb;
	uniform float cc;
	uniform vec3 lightDir2d;
	uniform float dd;
};
uniform_buffer CloudCrossSection2DConstants R13
{
	uniform vec4 rect;
};
#endif