//水彩画效果Shader
struct PSInput
{
	float4 m_v4Pos	: SV_POSITION;
	float2 m_v2UV	: TEXCOORD0;
};

cbuffer PerObjBuffer : register(b0)
{
	uint g_nFun;
	float g_fQuatLevel = 2.0f;    //量化bit数，取值2-6
	float g_fWaterPower = 40.0f;  //表示水彩扩展力度，单位为像素
};

Texture2D g_texture : register(t0);
Texture2D g_texNoise: register(t1);		//噪声纹理
SamplerState g_sampler : register(s0);

float4 Quant(float4 c4Color, float n)
{
	c4Color.x = int(c4Color.x * 255 / n) * n / 255;
	c4Color.y = int(c4Color.y * 255 / n) * n / 255;
	c4Color.z = int(c4Color.z * 255 / n) * n / 255;
	return c4Color;
}

float4 Watercolour(float2 v2UV, float2 v2TexSize)
{
	float4 c4NoiseColor
		= g_fWaterPower * g_texNoise.Sample(g_sampler, v2UV);
	float2 v2NewUV
		= float2(v2UV.x + c4NoiseColor.x / v2TexSize.x
			, v2UV.y + c4NoiseColor.y / v2TexSize.y);
	float4 c4Color = g_texture.Sample(g_sampler, v2NewUV);
	return Quant(c4Color, 255 / pow(2, g_fQuatLevel));
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float4 c4PixelColor;
	float2 v2TexSize;
	//读取纹理像素尺寸
	g_texture.GetDimensions(v2TexSize.x, v2TexSize.y);

	if (1 == g_nFun)
	{//水彩画效果
		c4PixelColor = Watercolour(input.m_v2UV, v2TexSize);
	}
	else
	{//默认原图
		c4PixelColor = g_texture.Sample(g_sampler, input.m_v2UV);
	}

	return c4PixelColor;
}
