struct PSInput
{
	float4 m_v4Pos	: SV_POSITION;
	float2 m_v2UV	: TEXCOORD0;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 Do_Filter(float3x3 mxFilter, float2 v2UV , float2 v2TexSize, Texture2D t2dTexture)
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

float4 GaussianFlur(float2 v2UV)
{//高斯模糊
	float2 v2TexSize;
	//读取纹理像素尺寸
	g_texture.GetDimensions(v2TexSize.x, v2TexSize.y);

	float2 v2PixelSite
		= float2(v2UV.x * v2TexSize.x, v2UV.y * v2TexSize.y);

	float3x3 mxOperators
		= float3x3(1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f
			, 2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f
			, 1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f);
	return Do_Filter(mxOperators, v2PixelSite, v2TexSize, g_texture);
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return GaussianFlur(input.m_v2UV);;
}
