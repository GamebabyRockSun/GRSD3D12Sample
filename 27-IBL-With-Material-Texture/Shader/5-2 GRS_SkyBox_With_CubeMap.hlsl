#include "0-1 HDR_COLOR_CONV.hlsli"
#include "0-3 GRS_Scene_CB_Def.hlsli"

TextureCube	g_txCubeMap : register(t0);
SamplerState g_smpLinear : register(s0);

struct ST_GRS_HLSL_VS_IN
{
    float4 m_v4LPos : POSITION;
};

struct ST_GRS_HLSL_PS_IN
{
    float4 m_v4LPos : SV_POSITION;
    float4 m_v4PPos : TEXCOORD0;
};

ST_GRS_HLSL_PS_IN SkyboxVS(ST_GRS_HLSL_VS_IN stVSInput)
{
    ST_GRS_HLSL_PS_IN stVSOutput;

    stVSOutput.m_v4LPos = stVSInput.m_v4LPos;
    stVSOutput.m_v4PPos = normalize(mul(stVSInput.m_v4LPos, g_mxInvVP));

    return stVSOutput;
}

float4 SkyboxPS(ST_GRS_HLSL_PS_IN stPSInput) : SV_TARGET
{
    float3 v3SRGBColor = LinearToSRGB(g_txCubeMap.Sample(g_smpLinear, stPSInput.m_v4PPos.xyz).rgb);
    return float4(v3SRGBColor,1.0f);
}
