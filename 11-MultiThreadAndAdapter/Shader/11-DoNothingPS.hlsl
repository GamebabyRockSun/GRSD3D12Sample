struct PSInput
{
	float4 m_v4Pos	: SV_POSITION;
	float2 m_v2UV	: TEXCOORD0;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.m_v2UV);
}
