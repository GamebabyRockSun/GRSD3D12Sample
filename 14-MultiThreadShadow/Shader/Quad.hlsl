struct PSInput
{
	float4 m_v4Position : SV_POSITION;
	float4 m_v4Color :COLOR;
	float2 m_v2UV : TEXCOORD;
};

cbuffer MVOBuffer : register(b0)
{
	float4x4 m_MVO;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 v4Position : POSITION, float4 v4Color : COLOR, float2 v2UV : TEXCOORD)
{
	PSInput result;

	result.m_v4Position = mul(v4Position, m_MVO);
	result.m_v4Color = v4Color;
	result.m_v2UV = v2UV;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	//return float4(1.0f,0.0f,0.0,1.0f);
	//return input.m_v4Color;

	// 采样阴影缓冲，并将其显示出来
	float d = g_texture.Sample(g_sampler, input.m_v2UV);
	return float4(d, d, d, 1.0f);
}
