struct PSInput
{
	float4 position : SV_POSITION;
	float4 color :COLOR;
	float2 uv : TEXCOORD;
};

cbuffer MVOBuffer : register(b0)
{
	float4x4 m_MVO;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float2 uv : TEXCOORD)
{
	PSInput result;

	result.position = mul(position,m_MVO);
	result.uv = uv;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.uv);
}
