#include "0-2 GRS_PBR_Function.hlsli"
#include "0-3 GRS_Scene_CB_Def.hlsli"

TextureCube  g_texHDREnvCubemap : register(t0);
SamplerState g_sapLinear		: register(s0);

struct ST_GRS_HLSL_PS_INPUT
{
	float4 m_v4HPos : SV_POSITION;
	float3 m_v4WPos : POSITION;
};

float4 PSMain(ST_GRS_HLSL_PS_INPUT pin) : SV_Target
{
	float3 N = normalize(pin.m_v4WPos.xyz);
	float3 R = N;
	float3 V = R;

	uint SAMPLE_COUNT = 4096u;
	float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
	float totalWeight = 0.0f;

	for (uint i = 0; i < SAMPLE_COUNT; ++i)
	{
		// 生成均匀分布的无偏序列（Hammersley）
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		// 进行有偏的重要性采样
		float3 H = ImportanceSampleGGX(Xi, N, g_fRoughness);
		//float3 H = ImportanceSampleGGX(Xi, N, 0.1f);

		float3 L = normalize(2.0f * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0f);

		if ( NdotL > 0.0f )
		{
			//float D = DistributionGGX(N, H, roughness);
			//float NdotH = max(dot(N, H), 0.0);
			//float HdotV = max(dot(H, V), 0.0);
			//float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

			//float resolution = 512.0; // resolution of source cubemap (per face)
			//float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
			//float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

			//float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

			//prefilteredColor += textureLod(g_texHDREnvCubemap, L, mipLevel).rgb * NdotL;
			//totalWeight += NdotL;

			prefilteredColor += g_texHDREnvCubemap.Sample(g_sapLinear, L).rgb * NdotL;
			totalWeight += NdotL;
		}
	}

	prefilteredColor = prefilteredColor / totalWeight;

	return float4(prefilteredColor, 1.0f);
}