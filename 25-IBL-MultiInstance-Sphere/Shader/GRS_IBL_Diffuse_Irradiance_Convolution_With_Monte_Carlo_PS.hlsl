
#include "GRS_PBR_Function.hlsli"

TextureCube  g_texHDREnvCubemap : register(t0);
SamplerState g_sapLinear		: register(s0);

struct ST_GRS_HLSL_PS_INPUT
{
    float4 m_v4HPos : SV_POSITION;
    float3 m_v4WPos : POSITION;
};

float2 UniformOnDisk(float Xi)
{
    float theta = TWO_PI * Xi;
    return float2(cos(theta), sin(theta));
}

float3 CosOnHalfSphere(float2 Xi)
{
    float   r = sqrt(Xi.x);
    float2  pInDisk = r * UniformOnDisk(Xi.y);
    float   z = sqrt(1 - Xi.x);
    return float3(pInDisk, z);
}

float4 Quarternion_ZTo(float3 to)
{
    // from = (0, 0, 1)
    //float cosTheta = dot(from, to);
    //float cosHalfTheta = sqrt(max(0, (cosTheta + 1) * 0.5));
    float cosHalfTheta = sqrt(max(0, (to.z + 1) * 0.5));
    //float3 axisSinTheta = cross(from, to);
    //    0    0    1
    // to.x to.y to.z
    //float3 axisSinTheta = float3(-to.y, to.x, 0);
    float twoCosHalfTheta = 2 * cosHalfTheta;
    return float4(-to.y / twoCosHalfTheta, to.x / twoCosHalfTheta, 0, cosHalfTheta);
}

float4 Quarternion_Inverse(float4 q) 
{
    return float4(-q.xyz, q.w);
}

float3 Quarternion_Rotate(float4 q, float3 p)
{
    // Quarternion_Mul(Quarternion_Mul(q, float4(p, 0)), Quarternion_Inverse(q)).xyz;

    float4 qp = float4(q.w * p + cross(q.xyz, p), -dot(q.xyz, p));
    float4 invQ = Quarternion_Inverse(q);
    float3 qpInvQ = qp.w * invQ.xyz + invQ.w * qp.xyz + cross(qp.xyz, invQ.xyz);
    return qpInvQ;
}

float3 CosOnHalfSphere(float2 Xi, float3 N)
{
    float3 p    = CosOnHalfSphere(Xi);
    float4 rot  = Quarternion_ZTo(N);
    return Quarternion_Rotate(rot, p);
}

float4 PSMain(ST_GRS_HLSL_PS_INPUT stPSInput) :SV_Target
{
    float3 N = normalize(stPSInput.m_v4WPos.xyz);

    float3 irradiance = float3(0.0f,0.0f,0.0f);

    const uint SAMPLE_NUM = GRS_INT_SAMPLES_CNT;

    for (uint i = 0u; i < SAMPLE_NUM; i++)
    {
        float2 Xi = Hammersley(i, SAMPLE_NUM);
        float3 L = CosOnHalfSphere(Xi, N);

        irradiance += g_texHDREnvCubemap.Sample(g_sapLinear, L).xyz;
    }

    irradiance *= 1.0 / float(SAMPLE_NUM);

    return float4(irradiance,1.0f);
}