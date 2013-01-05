TextureCube cubeTexture;
float4x4 worldViewProj	: WorldViewProjection;

SamplerCUBE samplerState 
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct a2v
{
    float4 position	: POSITION;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float3 texcoord		: TEXCOORD0;
};

v2f CubemapVS(a2v IN)
{
	v2f OUT;
    OUT.hPosition = mul( worldViewProj, float4(IN.position.xyz , 1.0));
	OUT.texcoord = normalize(IN.position.xyz);
    return OUT;
}

float4 CubemapPS(v2f IN) : SV_TARGET
{
	float4 c=cubeTexture.Sample(samplerState,IN.texcoord);
    return float4(c.rgb,1.f);
}

DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
	DepthFunc = LESS_EQUAL;
};

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};

technique11 simul_cubemap
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,CubemapVS()));
		SetPixelShader(CompileShader(ps_4_0,CubemapPS()));
    }
}
