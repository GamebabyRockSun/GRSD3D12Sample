

cbuffer cbPerObject : register( b0 )
{
    row_major matrix    g_mViewProjection;
    row_major matrix    g_mWorld;
    float4              g_v4EyePos;
}

TextureCube	g_EnvironmentTexture : register( t0 );
SamplerState g_Sample : register( s0 );

struct SkyboxVS_Input
{
    float4 Pos : POSITION;
};

struct SkyboxVS_Output
{
    float4 vPos : POSITION;
    float4 sPos : SV_POSITION;
};

SkyboxVS_Output SkyboxVS( SkyboxVS_Input Input )
{
    SkyboxVS_Output Output;
    
    // Use local vertex position as cubemap lookup vector.
    Output.vPos = Input.Pos;

    // Transform to world space.
    float4 posW = mul(Input.Pos, g_mWorld);

    // Always center sky about camera.
    posW.xyz += g_v4EyePos.xyz;

    // Set z = w so that z/w = 1 (i.e., skydome always on far plane).
    Output.sPos = mul(posW, g_mViewProjection).xyww;

    return Output;
}

float4 SkyboxPS( SkyboxVS_Output Input ) : SV_TARGET
{
	return g_EnvironmentTexture.Sample(g_Sample, Input.vPos);
}
