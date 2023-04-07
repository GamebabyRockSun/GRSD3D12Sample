#include "GRS_Normal_Map_CB_Def.hlsli"

struct ST_GRS_HLSL_PS_INPUT
{
    float4 m_v4HPos			: SV_POSITION;
    float4 m_v4WPos         : POSITION;
    float2 m_v2UV			: TEXCOORD;

    float3 m_v3WTangent     : COLOR0;
    float3 m_v3WBitangent   : COLOR1;
    float3 m_v3WNormal      : COLOR2;
};

Texture2D g_texColor        : register(t0);      // 纹理（BaseColor)
Texture2D g_texNormal       : register(t1);      // 法线

SamplerState g_sapLinear      : register(s0);

float4 PSMain(ST_GRS_HLSL_PS_INPUT stPSInput) : SV_TARGET
{
    // 逐像素法线
    float3 N = g_texNormal.Sample(g_sapLinear, stPSInput.m_v2UV).xyz;
    N = 2.0f * N - 1.0f;
    N = normalize(N);
    
    float3x3 mxTBN= { stPSInput.m_v3WTangent , stPSInput.m_v3WBitangent, stPSInput.m_v3WNormal };
    N = normalize(mul(N,mxTBN));
    float3 v3LightDir = normalize( g_v4LightPos.xyz - stPSInput.m_v4WPos.xyz );    
    float3 v3ViewDir = normalize( g_v4EyePos.xyz - stPSInput.m_v4WPos.xyz );

    //return float4(N, 1.0f);

    // Base Color    
    float3 v3Color = g_texColor.Sample(g_sapLinear, stPSInput.m_v2UV).rgb;
    // Ambient
    float3 v3Ambient = 0.1 * v3Color;
    // Diffuse
    float fFiff = max(dot(v3LightDir, N), 0.0);
    float3 v3Diffuse = fFiff * v3Color;
    // Specular
    float3 v3ReflectDir = reflect(-v3LightDir, N);
    float3 v3HalfwayDir = normalize(v3LightDir + v3ViewDir);
    float  fSpec = pow(max(dot(N, v3HalfwayDir), 0.0), 32.0);
    float3 v3Specular = float3(0.2f,0.2f,0.2f) * fSpec;

    return float4(v3Ambient + v3Diffuse + v3Specular, 1.0f);
}