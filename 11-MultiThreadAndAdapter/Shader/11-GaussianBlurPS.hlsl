//这个Shader改编自微软官方D3D12示例，删除了不必要的VS函数，以及啰嗦的为测试性能而编造的假循环

struct PSInput
{
	float4 m_v4Position : SV_POSITION;
	float2 m_v2UV : TEXCOORD;
};

static const float KernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };
static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };

// The input texture to blur.
Texture2D g_Texture : register(t0);
SamplerState g_LinearSampler : register(s0);

// Simple gaussian blur in the vertical direction.
float4 PSSimpleBlurV(PSInput input) : SV_TARGET
{
	float3 textureColor = float3(1.0f, 0.0f, 0.0f);
	float2 m_v2UV = input.m_v2UV;
	float2 v2TexSize;
	//读取纹理像素尺寸
	g_Texture.GetDimensions(v2TexSize.x, v2TexSize.y);

	textureColor = g_Texture.Sample(g_LinearSampler, m_v2UV).xyz * BlurWeights[0];
	for (int i = 1; i < 3; i++)
	{
		float2 normalizedOffset = float2(0.0f, KernelOffsets[i]) / v2TexSize.y;
		textureColor += g_Texture.Sample(g_LinearSampler, m_v2UV + normalizedOffset).xyz * BlurWeights[i];
		textureColor += g_Texture.Sample(g_LinearSampler, m_v2UV - normalizedOffset).xyz * BlurWeights[i];
	}
	return float4(textureColor, 1.0);
}
// Simple gaussian blur in the horizontal direction.
float4 PSSimpleBlurU(PSInput input) : SV_TARGET
{
	float3 textureColor = float3(1.0f, 0.0f, 0.0f);
	float2 m_v2UV = input.m_v2UV;
	float2 v2TexSize;
	//读取纹理像素尺寸
	g_Texture.GetDimensions(v2TexSize.x, v2TexSize.y);

	textureColor = g_Texture.Sample(g_LinearSampler, m_v2UV).xyz * BlurWeights[0];
	for (int i = 1; i < 3; i++)
	{
		float2 normalizedOffset = float2(KernelOffsets[i], 0.0f) / v2TexSize.x;
		textureColor += g_Texture.Sample(g_LinearSampler, m_v2UV + normalizedOffset).xyz * BlurWeights[i];
		textureColor += g_Texture.Sample(g_LinearSampler, m_v2UV - normalizedOffset).xyz * BlurWeights[i];
	}
	return float4(textureColor, 1.0);
}
