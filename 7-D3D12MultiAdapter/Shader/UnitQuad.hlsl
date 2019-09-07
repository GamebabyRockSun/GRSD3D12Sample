struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
{
	PSInput result;

	result.position = position;
	result.uv = uv;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	//简单实现一个黑白效果
	//float4 _inColor = g_texture.Sample(g_sampler, input.uv);
	//float h = 0.3 * _inColor.x + 0.59 * _inColor.y + 0.11 * _inColor.x;
	//float4 _outColor = float4(h, h, h, 1.0);
	//return _outColor;

	////实现浮雕效果
	//float2 f2TexSize = float2(1024.0, 768.0); //Texture 大小 最好从参数传入

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
