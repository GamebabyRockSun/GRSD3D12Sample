cbuffer CB_GRS_SCENE_MATRIX : register(b0)
{
    row_major matrix    g_mxInvWVP;
}

TextureCube	g_txCubeMap : register(t0);
SamplerState g_smpLinear : register(s0);

struct ST_GRS_HLSL_VS_IN
{
    float4 m_v4LPos : POSITION;
};

struct ST_GRS_HLSL_PS_IN
{
    float4 m_v4WPos : SV_POSITION;
    float3 m_v3Tex  : TEXCOORD0;
};

ST_GRS_HLSL_PS_IN SkyboxVS(ST_GRS_HLSL_VS_IN stVSInput)
{
    ST_GRS_HLSL_PS_IN stVSOutput;

    stVSOutput.m_v4WPos = stVSInput.m_v4LPos;
    stVSOutput.m_v3Tex = normalize(mul(stVSInput.m_v4LPos, g_mxInvWVP).xyz);

    return stVSOutput;
}

float4 SkyboxPS(ST_GRS_HLSL_PS_IN stPSInput) : SV_TARGET
{
    return g_txCubeMap.Sample(g_smpLinear, stPSInput.m_v3Tex);
}
