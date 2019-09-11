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
	////实现浮雕效果
	//float2 f2TexSize = float2(512.0,512.0); //Texture 大小 最好从参数传入

	//float2 upLeftUV = float2(input.uv.x - 1.0 / f2TexSize.x, input.uv.y - 1.0 / f2TexSize.y);
	//float4 bkColor = float4(0.5, 0.5, 0.5, 1.0);
	//float4 curColor = g_texture.Sample(g_sampler, input.uv);
	//float4 upLeftColor = g_texture.Sample(g_sampler, upLeftUV);

	//float4 delColor = curColor - upLeftColor;

	//float h = 0.3 * delColor.x + 0.59 * delColor.y + 0.11 * delColor.z;
	//float4 _outColor = float4(h, h, h, 0.0) + bkColor;
	//return _outColor;

	return g_texture.Sample(g_sampler, input.uv);
}
