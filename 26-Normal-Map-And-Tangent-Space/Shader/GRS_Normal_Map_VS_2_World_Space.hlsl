#include "GRS_Normal_Map_CB_Def.hlsli"

struct ST_GRS_HLSL_VS_INPUT
{
    float4 m_v4LPos			: POSITION;
    float4 m_v4LNormal		: NORMAL;
    float2 m_v2UV			: TEXCOORD;
    float4 m_v4LTangent		: TANGENT;
};

struct ST_GRS_HLSL_VS_OUTPUT
{
    float4 m_v4HPos			: SV_POSITION;
    float4 m_v4WPos         : POSITION;
    float2 m_v2UV			: TEXCOORD;

    float3 m_v3WTangent      : COLOR0;
    float3 m_v3WBitangent    : COLOR1;
    float3 m_v3WNormal       : COLOR2;
};

ST_GRS_HLSL_VS_OUTPUT VSMain(ST_GRS_HLSL_VS_INPUT stVSInput)
{
    ST_GRS_HLSL_VS_OUTPUT stVSOutput;

    stVSOutput.m_v4HPos = mul(stVSInput.m_v4LPos, g_mxModel2World);
    stVSOutput.m_v4HPos = mul(stVSOutput.m_v4HPos, g_mxWVP);
    stVSOutput.m_v4WPos = mul(stVSInput.m_v4LPos, g_mxModel2World);
    stVSOutput.m_v4WPos = mul(stVSOutput.m_v4WPos, g_mxWorld);

    stVSOutput.m_v2UV = stVSInput.m_v2UV;

    // 施密特正交法计算切空间的TBN基坐标
    stVSInput.m_v4LTangent.w = 0.0f;
    stVSOutput.m_v3WTangent
        = normalize(mul(stVSInput.m_v4LTangent, g_mxModel2World).xyz);
    stVSInput.m_v4LNormal.w = 0.0f;
    stVSOutput.m_v3WNormal
        = normalize(mul(stVSInput.m_v4LNormal, g_mxModel2World).xyz);
    stVSOutput.m_v3WTangent
        = normalize(stVSOutput.m_v3WTangent - dot(stVSOutput.m_v3WTangent, stVSOutput.m_v3WNormal) * stVSOutput.m_v3WNormal);
    stVSOutput.m_v3WBitangent
        = normalize(cross(stVSOutput.m_v3WTangent, stVSOutput.m_v3WNormal));

    return stVSOutput;
}
