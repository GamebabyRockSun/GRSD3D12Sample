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
	float g_fQuatLevel = 2.0f;    //量化bit数，取值2-6
	float g_fWaterPower = 40.0f;  //表示水彩扩展力度，单位为像素
};

Texture2D g_texture : register(t0);
Texture2D g_texNoise: register(t1);		//噪声纹理
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
	float2 upLeftUV = float2(input.m_v2UV.x - 1.0 / g_v2TexSize.x
		, input.m_v2UV.y - 1.0 / g_v2TexSize.y);
	float4 bkColor = float4(0.5, 0.5, 0.5, 1.0);
	float4 curColor = g_texture.Sample(g_sampler, input.m_v2UV);
	float4 upLeftColor = g_texture.Sample(g_sampler, upLeftUV);

	float4 delColor = curColor - upLeftColor;

	float h = 0.3 * delColor.x + 0.59 * delColor.y + 0.11 * delColor.z;
	float4 _outColor = float4(h, h, h, 0.0) + bkColor;
	return _outColor;
}

static float2 g_v2MosaicSize1 = float2(32.0f, 32.0f);
float4 Mosaic1(PSInput input)
{//方形马赛克
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
{//圆形马赛克
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

float4 Do_Filter(float3x3 mxFilter,float2 v2UV,float2 v2TexSize, Texture2D t2dTexture)
{//根据滤波矩阵计算“九宫格”形式像素的滤波结果的函数
	float2 v2aUVDelta[3][3]
		= {
			{ float2(-1.0f,-1.0f), float2(0.0f,-1.0f), float2(1.0f,-1.0f) },
			{ float2(-1.0f,0.0f),  float2(0.0f,0.0f),  float2(1.0f,0.0f)  },
			{ float2(-1.0f,1.0f),  float2(0.0f,1.0f),  float2(1.0f,1.0f)  },
		};
	
	float4 c4Color = float4(0.0f, 0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			//计算采样点，得到当前像素附件的像素点的坐标（上下左右，八方）
			float2 v2NearbySite
				= v2UV + v2aUVDelta[i][j];
			float2 v2NearbyUV
				= float2(v2NearbySite.x / v2TexSize.x, v2NearbySite.y / v2TexSize.y);
			c4Color += (t2dTexture.Sample(g_sampler, v2NearbyUV) * mxFilter[i][j]);
		}
	}
	return c4Color;
}

float4 BoxFlur(float2 v2UV,float2 v2TexSize)
{//简单的9点Box模糊
	float2 v2PixelSite 
		= float2(v2UV.x * v2TexSize.x, v2UV.y * v2TexSize.y);
	float3x3 mxOperators 
		= 1.0f/9.0f * float3x3(1.0f, 1.0f, 1.0f
					, 1.0f, 1.0f, 1.0f
					, 1.0f, 1.0f, 1.0f);
	return Do_Filter(mxOperators, v2PixelSite, v2TexSize, g_texture);
}

float4 GaussianFlur(float2 v2UV, float2 v2TexSize)
{//高斯模糊
	float2 v2PixelSite
		= float2(v2UV.x * v2TexSize.x, v2UV.y * v2TexSize.y);
	float3x3 mxOperators
		= float3x3(1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f
			, 2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f
			, 1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f);
	return Do_Filter(mxOperators, v2PixelSite, v2TexSize, g_texture);
}

float4 LaplacianSharpen(float2 v2UV, float2 v2TexSize)
{//拉普拉斯锐化
	float2 v2PixelSite
		= float2(v2UV.x * v2TexSize.x, v2UV.y * v2TexSize.y);
	float3x3 mxOperators
		= float3x3(-1.0f, -1.0f, -1.0f
			, -1.0f, 9.0f, -1.0f
			, -1.0f, -1.0f, -1.0f);
	return Do_Filter(mxOperators, v2PixelSite, v2TexSize, g_texture);
}

float4 Contour(float2 v2UV, float2 v2TexSize)
{//描边
	float2 v2PixelSite
		= float2(v2UV.x * v2TexSize.x, v2UV.y * v2TexSize.y);
	float3x3 mxOperators
		= float3x3(-0.5f, -1.0f, 0.0f
			, -1.0f, 0.0f, 1.0f
			, -0.0f, 1.0f, 0.5f);
	float4 c4Color = Do_Filter(mxOperators, v2PixelSite, v2TexSize, g_texture);
	
	float fDeltaGray = 0.3f * c4Color.x + 0.59 * c4Color.y + 0.11 * c4Color.z;

	//fDeltaGray += 0.5f;
	
	////OR

	if (fDeltaGray < 0.0f)
	{
		fDeltaGray = -1.0f * fDeltaGray;
	}
	fDeltaGray = 1.0f - fDeltaGray;

	return float4(fDeltaGray, fDeltaGray, fDeltaGray, 1.0f);
}

float4 SobelAnisotropyContour(float2 v2UV, float2 v2TexSize)
{//各向异性索博尔描边
	float2 v2PixelSite
		= float2(v2UV.x * v2TexSize.x, v2UV.y * v2TexSize.y);
	float3x3 mxOperators1
		= float3x3(  -1.0f, -2.0f, -1.0f
					, 0.0f, 0.0f, 0.0f
					, 1.0f, 2.0f, 1.0f
			      );
	float3x3 mxOperators2
		= float3x3(	 -1.0f, 0.0f, 1.0f
					,-2.0f, 0.0f, 2.0f
					,-1.0f, 0.0f, 1.0f
				  );

	float4 c4Color1 = Do_Filter(mxOperators1, v2PixelSite, v2TexSize, g_texture);
	//float4 c4Color2 = Do_Filter(mxOperators2, v2PixelSite, v2TexSize, g_texture);

	//c4Color1 += c4Color2;

	////计算颜色溢色的简单方法
	//float fMaxColorComponent 
	//	= max(c4Color1.x
	//	, max(c4Color1.y
	//		, c4Color1.z));
	//float fMinColorComponent 
	//	= min(
	//	min(c4Color1.x
	//		, min(c4Color1.y
	//			, c4Color1.z))
	//	,0.0f);

	//c4Color1 = (c4Color1 - float4(fMinColorComponent
	//	, fMinColorComponent
	//	, fMinColorComponent
	//	, fMinColorComponent))
	//	/(fMaxColorComponent- fMinColorComponent);

	float fDeltaGray = 0.3f * c4Color1.x + 0.59 * c4Color1.y + 0.11 * c4Color1.z;

	if (fDeltaGray < 0.0f)
	{
		fDeltaGray = -1.0f * fDeltaGray;
	}
	fDeltaGray = 1.0f - fDeltaGray;

	return float4(fDeltaGray, fDeltaGray, fDeltaGray, 1.0f);
}

float4 SobelIsotropyContour(float2 v2UV, float2 v2TexSize)
{//各向同性索博尔描边
	float2 v2PixelSite
		= float2(v2UV.x * v2TexSize.x, v2UV.y * v2TexSize.y);
	float3x3 mxOperators1
		= float3x3(  -1.0f, -1.414214f, -1.0f
					, 0.0f, 0.0f, 0.0f
					, 1.0f, 1.414214f, 1.0f
					);
	float3x3 mxOperators2
		= float3x3(   -1.0f, 0.0f, 1.0f
					, -1.414214f, 0.0f, 1.414214f
					, -1.0f, 0.0f, 1.0f
					);

	float4 c4Color1 = Do_Filter(mxOperators1, v2PixelSite, v2TexSize, g_texture);
	float4 c4Color2 = Do_Filter(mxOperators2, v2PixelSite, v2TexSize, g_texture);

	c4Color1 += c4Color2;

	float fDeltaGray = 0.3f * c4Color1.x + 0.59 * c4Color1.y + 0.11 * c4Color1.z;

	if (fDeltaGray < 0.0f)
	{
		fDeltaGray = -1.0f * fDeltaGray;
	}
	fDeltaGray = 1.0f - fDeltaGray;

	return float4(fDeltaGray, fDeltaGray, fDeltaGray, 1.0f);
}

//float g_fQuatLevel = 2;    //量化bit数，取值2-6
//float g_fWaterPower = 40;  //表示水彩扩展力度，单位为像素
//Texture2D g_texture : register(t0);
//Texture2D g_texNoise: register(t1);		//噪声纹理

float4 Quant(float4 c4Color, float n)
{
	c4Color.x = int(c4Color.x * 255 / n) * n / 255;
	c4Color.y = int(c4Color.y * 255 / n) * n / 255;
	c4Color.z = int(c4Color.z * 255 / n) * n / 255;
	return c4Color;
}

float4 Watercolour(float2 v2UV,float2 v2TexSize)
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

	if (0 == g_nFun)
	{//原图
		c4PixelColor = g_texture.Sample(g_sampler, input.m_v2UV);
	}
	else if( 1 == g_nFun )
	{//黑白效果
		c4PixelColor = g_texture.Sample(g_sampler, input.m_v2UV);
		c4PixelColor = BlackAndWhitePhoto(c4PixelColor);
	}
	else if( 2 == g_nFun )
	{//浮雕效果
		c4PixelColor = Anaglyph(input);
	}
	else if( 3 == g_nFun )
	{//方块马赛克
		c4PixelColor = Mosaic1(input);
	}
	else if (4 == g_nFun)
	{//圆片马赛克
		c4PixelColor = Mosaic2(input);
	}
	else if (5 == g_nFun)
	{//Box模糊，9点颜色平均值
		c4PixelColor = BoxFlur(input.m_v2UV, g_v2TexSize);
	}
	else if (6 == g_nFun)
	{//高斯模糊
		c4PixelColor = GaussianFlur(input.m_v2UV, g_v2TexSize);
	}
	else if (7 == g_nFun)
	{//拉普拉斯锐化
		c4PixelColor = LaplacianSharpen(input.m_v2UV, g_v2TexSize);
	}
	else if (8 == g_nFun)
	{//描边
		c4PixelColor = Contour(input.m_v2UV, g_v2TexSize);
	}
	else if (9 == g_nFun)
	{//索博尔各向异性描边
		c4PixelColor = SobelAnisotropyContour(input.m_v2UV, g_v2TexSize);
	}	
	else if (10 == g_nFun)
	{//索博尔各向同性描边
		c4PixelColor = SobelIsotropyContour(input.m_v2UV, g_v2TexSize);
	}
	else if (11 == g_nFun)
	{//水彩画效果
		c4PixelColor = Watercolour(input.m_v2UV, g_v2TexSize);
	}
	else
	{//默认原图
		c4PixelColor = g_texture.Sample(g_sampler, input.m_v2UV);
	}

	return c4PixelColor;
}
