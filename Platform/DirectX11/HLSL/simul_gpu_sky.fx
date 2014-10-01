#include "CppHlsl.hlsl"
uniform sampler2D input_texture;
uniform sampler2D density_texture;
uniform sampler3D loss_texture;
uniform sampler3D insc_texture;
uniform sampler2D optical_depth_texture;
uniform sampler2D blackbody_texture;
RWTexture3D<vec4> targetTexture;

SamplerState samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
	AddressW = Clamp;
};
#include "../../CrossPlatform/SL/states.sl"
#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "../../CrossPlatform/SL/simul_gpu_sky.sl"

struct vertexInput
{
    float3 position		: POSITION;
    vec2 texCoords	: TEXCOORD0;
};

struct vertexOutput
{
    vec4 hPosition	: SV_POSITION;
	vec2 texCoords	: TEXCOORD0;		
};

vertexOutput VS_Main(idOnly IN)
{
    vertexOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	pos.y			=yRange.x+pos.y*yRange.y;
	vec4 vert_pos	=vec4(vec2(-1.0,1.0)+2.0*vec2(pos.x,-pos.y),1.0,1.0);
    OUT.hPosition	=vert_pos;
    OUT.texCoords	=pos;
    return OUT;
}

vec4 PS_Loss(vertexOutput IN) : SV_TARGET
{
	return PSLoss(input_texture,density_texture,IN.texCoords.xy);
}

CS_LAYOUT(8,1,1)
void CS_Loss(uint3 sub_pos	: SV_DispatchThreadID )
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	uint linear_pos		=sub_pos.x+threadOffset.x;
	uint3 pos			=LinearThreadToPos2D(linear_pos,dims);
	CSLoss(targetTexture,density_texture,pos,maxOutputAltKm,maxDistanceKm,maxDensityAltKm,targetSize);
}

CS_LAYOUT(1,1,1)
void CS_LightTable( uint3 sub_pos : SV_DispatchThreadID )
{
	MakeLightTable(targetTexture,insc_texture,optical_depth_texture,sub_pos,targetSize);
}

CS_LAYOUT(8,1,1)
void CS_Insc( uint3 sub_pos : SV_DispatchThreadID )
{
	int3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	uint linear_pos			=sub_pos.x+threadOffset.x;
	
	int3 pos				=LinearThreadToPos2D(linear_pos,dims);
	if(pos.x>=dims.x||pos.y>=dims.y)
		return;
	
	vec2 texc				=(pos.xy+0.5)/vec2(dims.xy);

	vec4 previous_insc		=vec4(0.0,0.0,0.0,0.0);
	float sin_e				=max(-1.0,min(1.0,1.0-2.0*(texc.y*texSize.y-texelOffset)/(texSize.y-1.0)));
	float cos_e				=sqrt(1.0-sin_e*sin_e);
	float altTexc			=(texc.x*texSize.x-texelOffset)/max(texSize.x-1.0,1.0);
	float viewAltKm			=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm		=getDistanceToSpace(sin_e,viewAltKm);
	
	float prevDist_km		=0.0;
	
	targetTexture[pos]		=previous_insc;
	vec3 mie_factor			=vec3(1.0,1.0,1.0);
	for(int i=1;i<dims.z;i++)
	{
		uint3 idx			=uint3(pos.xy,i);
		float zPosition		=pow((float)(i)/((float)dims.z-1.0),2.0);
		
		vec3 previous_loss	=loss_texture[idx].rgb;

		float dist_km		=zPosition*maxDistanceKm;
		if(i==dims.z-1)
			dist_km=1000.0;
		float maxd			=min(spaceDistKm,dist_km);
		float mind			=min(spaceDistKm,prevDist_km);
		float dist			=0.5*(mind+maxd);
		float stepLengthKm	=max(0.0,maxd-mind);
		float y				=planetRadiusKm+viewAltKm+dist*sin_e;
		float x				=dist*cos_e;
		float r				=sqrt(x*x+y*y);
		float alt_km		=r-planetRadiusKm;
	
		float x1			=mind*cos_e;
		float r1			=sqrt(x1*x1+y*y);
		float alt_1_km		=r1-planetRadiusKm;
	
		float x2			=maxd*cos_e;
		float r2			=sqrt(x2*x2+y*y);
		float alt_2_km		=r2-planetRadiusKm;
	
		// lookups is: dens_factor,ozone_factor,haze_factor;
		float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
		vec4 lookups		=texture_clamp_lod(density_texture,dens_texc,0);
		float dens_factor	=lookups.x;
		float ozone_factor	=lookups.y;
		float haze_factor	=getHazeFactorAtAltitude(alt_km);
		vec4 light			=vec4(sunIrradiance,1.0)*getSunlightFactor(optical_depth_texture,alt_km,lightDir);
		light.rgb			*=RAYLEIGH_BETA_FACTOR;
		vec4 insc			=light;

		vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
		vec3 total_ext		=extinction+ozone*ozone_factor;
		vec3 loss;
		loss				=exp(-extinction*stepLengthKm);
		insc.rgb			*=vec3(1.0,1.0,1.0)-loss;
		mie_factor			*=exp(-insc.w*stepLengthKm*haze_factor*hazeMie);
	
		insc.rgb			*=previous_loss.rgb;
		insc.rgb			+=previous_insc.rgb;

		insc.w				=saturate((1.0-mie_factor.x)/(1.0-previous_loss.x+0.0001f));
		targetTexture[idx]	=vec4(insc.rgb,insc.a);
		prevDist_km			=dist_km;
		previous_insc		=insc;
	}
}

CS_LAYOUT(8,1,1)
void CS_Skyl( uint3 sub_pos : SV_DispatchThreadID )
{
	uint3 dims;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	uint linear_pos		=sub_pos.x+threadOffset.x;
	uint3 pos			=LinearThreadToPos2D(linear_pos,dims);
	CSSkyl(targetTexture,loss_texture,insc_texture,density_texture,blackbody_texture,pos,maxOutputAltKm,maxDistanceKm,maxDensityAltKm,targetSize);
	
}

vec4 PS_Insc(vertexOutput IN) : SV_TARGET
{
	return Insc(input_texture,loss_texture,density_texture,optical_depth_texture,IN.texCoords);
}

vec4 PS_Skyl(vertexOutput IN) : SV_TARGET
{
	vec4 previous_skyl	=texture(input_texture,IN.texCoords.xy);
	vec3 previous_loss	=texture(loss_texture,vec3(IN.texCoords.xy,pow(distanceKm/maxDistanceKm,0.5))).rgb;
	// should adjust texCoords - we want the PREVIOUS loss!
	float sin_e			=clamp(1.0-2.0*(IN.texCoords.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(IN.texCoords.x*texSize.x-texelOffset)/(texSize.x-1.0);
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
	vec4 lookups		=texture(density_texture,dens_texc);
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
	skyl.w				=saturate((1.0-mie_factor)/(1.0-total_ext.x+0.0001f));
	
	//skyl.w				=(loss.w)*(1.0-previous_skyl.w)*skyl.w+previous_skyl.w;
	skyl.rgb			*=previous_loss.rgb;
	skyl.rgb			+=previous_skyl.rgb;
	float lossw=1.0;
	skyl.w				=(lossw)*(1.0-previous_skyl.w)*skyl.w+previous_skyl.w;
    return skyl;
}


//------------------------------------
// Technique
//------------------------------------
DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};
BlendState DontBlend
{
	BlendEnable[0] = FALSE;
};
RasterizerState RenderNoCull { CullMode = none; };

technique11 simul_gpu_loss
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Loss()));
    }
}

technique11 simul_gpu_insc
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Insc()));
    }
}

technique11 simul_gpu_skyl
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend,vec4( 0.0, 0.0, 0.0, 0.0 ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_Skyl()));
    }
}
technique11 gpu_light_table_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_LightTable()));
    }
}

technique11 gpu_loss_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Loss()));
    }
}

technique11 gpu_insc_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Insc()));
    }
}

technique11 gpu_skyl_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Skyl()));
    }
}