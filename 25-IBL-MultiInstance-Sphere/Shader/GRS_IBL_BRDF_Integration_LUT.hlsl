
#include "GRS_PBR_Function.hlsli"

struct ST_GRS_HLSL_VS_IN
{
	float4 m_v4LPos : POSITION;
	float2 m_v2UV	: TEXCOORD;
};

struct ST_GRS_HLSL_VS_OUT
{
	float4 m_v4HPos : SV_POSITION;
	float2 m_v2UV	: TEXCOORD;
};

ST_GRS_HLSL_VS_OUT VSMain(ST_GRS_HLSL_VS_IN stVSIn)
{
	ST_GRS_HLSL_VS_OUT stVSOut = (ST_GRS_HLSL_VS_OUT)0.0f;

	stVSOut.m_v2UV = stVSIn.m_v2UV;
	//to clip space
	stVSOut.m_v4HPos = stVSIn.m_v4LPos;

	return stVSOut;
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
	float3 V;
	V.x = sqrt(1.0f - NdotV * NdotV);
	V.y = 0.0f;
	V.z = NdotV;

	float A = 0.0f;
	float B = 0.0f;

	float3 N = float3(0.0f, 0.0f, 1.0f);

	uint SAMPLE_COUNT = GRS_INT_SAMPLES_CNT;

	for (uint i = 0; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, N, roughness);
		float3 L = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(L.z, 0.0);
		float NdotH = max(H.z, 0.0);
		float VdotH = max(dot(V, H), 0.0);

		if (NdotL > 0.0)
		{
			float G = GeometrySmith_IBL(N, V, L, roughness);
			float G_Vis = (G * VdotH) / (NdotH * NdotV);
			float Fc = pow(1.0 - VdotH, 5.0);

			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	A /= float(SAMPLE_COUNT);
	B /= float(SAMPLE_COUNT);

	return float2(A, B);
}

float2 PSMain(ST_GRS_HLSL_VS_OUT pin) :SV_TARGET
{
	return IntegrateBRDF(pin.m_v2UV.x, pin.m_v2UV.y);
}