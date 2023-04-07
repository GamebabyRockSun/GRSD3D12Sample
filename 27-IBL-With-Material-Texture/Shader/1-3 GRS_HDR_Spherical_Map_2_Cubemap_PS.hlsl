#include "0-2 GRS_PBR_Function.hlsli"

Texture2D    g_txHDR        : register(t0);
SamplerState g_smpLinear    : register(s0);

struct ST_GRS_HLSL_PS_IN
{
    float4 m_v4HPos     : SV_POSITION;              // Projection coord
    float4 m_v4WPos		: POSITION;                 // World position
};

float4 PSMain(ST_GRS_HLSL_PS_IN stPSInput) : SV_Target
{
    float2 v2UV = SampleSphericalMap(normalize(stPSInput.m_v4WPos.xyz));
    return float4(g_txHDR.Sample(g_smpLinear, v2UV).rgb, 1.0f);
}
