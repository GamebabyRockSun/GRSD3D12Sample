struct PSInput
{
	float4 m_v4Pos	: SV_POSITION;
	float2 m_v2UV	: TEXCOORD0;
	float3 m_v3Nor	: NORMAL;
};

cbuffer MVPBuffer : register(b0)
{
	float4x4 g_mxMVP;
};

cbuffer PerObjBuffer : register(b1)
{
	uint g_nFun;
	float2 g_v2TexSize;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 v4Pos : POSITION, float2 v2UV : TEXCOORD0,float3 v3Nor:NORMAL )
{
	PSInput stResult;
	stResult.m_v4Pos = mul(v4Pos, g_mxMVP);
	stResult.m_v2UV = v2UV;
	stResult.m_v3Nor = v3Nor;
	return stResult;
}

float4 BlackAndWhitePhoto(float4 inColor)
{
	float BWColor = 0.3f * inColor.x + 0.59f * inColor.y + 0.11f * inColor.z;
	return float4(BWColor, BWColor, BWColor, 1.0f);
}

float4 Anaglyph(PSInput input)
{
	//实现浮雕效果
	float2 upLeftUV = float2(input.m_v2UV.x - 1.0 / g_v2TexSize.x, input.m_v2UV.y - 1.0 / g_v2TexSize.y);
	float4 bkColor = float4(0.5, 0.5, 0.5, 1.0);
	float4 curColor = g_texture.Sample(g_sampler, input.m_v2UV);
	float4 upLeftColor = g_texture.Sample(g_sampler, upLeftUV);

	float4 delColor = curColor - upLeftColor;

	float h = 0.3 * delColor.x + 0.59 * delColor.y + 0.11 * delColor.z;
	float4 _outColor = float4(h, h, h, 0.0) + bkColor;
	return delColor + bkColor;
	return _outColor;
}

static float2 g_v2MosaicSize1 = float2(8.0f, 8.0f);
float4 Mosaic1(PSInput input)
{
	float2 v2PixelSite 
		= float2(input.m_v2UV.x * g_v2TexSize.x
			, input.m_v2UV.y * g_v2TexSize.y);

	float2 v2NewUV 
		= float2(int(v2PixelSite.x / g_v2MosaicSize1.x) * g_v2MosaicSize1.x
			, int(v2PixelSite.y / g_v2MosaicSize1.y) * g_v2MosaicSize1.y);

	v2NewUV /= g_v2TexSize;
	return g_texture.Sample(g_sampler, v2NewUV);
}

static float2 g_v2MosaicSize2 = float2(16.0f, 16.0f);
float4 Mosaic2(PSInput input)
{
	float2 v2PixelSite
		= float2(input.m_v2UV.x * g_v2TexSize.x
			, input.m_v2UV.y * g_v2TexSize.y);

	//新的纹理坐标取到中心点
	float2 v2NewUV
		= float2(int( v2PixelSite.x / g_v2MosaicSize2.x) * g_v2MosaicSize2.x
			, int(v2PixelSite.y / g_v2MosaicSize2.y) * g_v2MosaicSize2.y)
		+ 0.5 * g_v2MosaicSize2;

	float2 v2DeltaUV = v2NewUV - v2PixelSite;
	float fDeltaLen = length(v2DeltaUV);

	float2 v2MosaicUV = float2( v2NewUV.x / g_v2TexSize.x,v2NewUV.y / g_v2TexSize.y );

	float4 c4Color;
	//判断新的UV点是否在圆心内
	if (fDeltaLen < 0.5 * g_v2MosaicSize2.x)
	{
		c4Color = g_texture.Sample(g_sampler, v2MosaicUV);
	}
	else
	{
		c4Color = g_texture.Sample(g_sampler, input.m_v2UV);
	}

	return c4Color;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float4 c4PixelColor;
	if (0 == g_nFun)
	{//黑白效果
		c4PixelColor = g_texture.Sample(g_sampler, input.m_v2UV);
		c4PixelColor = BlackAndWhitePhoto(c4PixelColor);
	}
	else if( 1 == g_nFun )
	{//浮雕效果
		c4PixelColor = Anaglyph(input);
	}
	else if( 2 == g_nFun )
	{//方块马赛克
		c4PixelColor = Mosaic1(input);
	}
	else if( 3 == g_nFun )
	{//圆片马赛克
		c4PixelColor = Mosaic2(input);
	}
	else
	{//原图
		c4PixelColor = g_texture.Sample(g_sampler, input.m_v2UV);
	}


	return c4PixelColor;
}
