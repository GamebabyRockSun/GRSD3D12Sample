
cbuffer ST_GRS_GIF_FRAME_PARAM : register(b0)
{
	uint m_nFrame;
	uint m_nDisposal;
	float4 m_c4BkColor;
};

Texture2D			g_tGIFFrame : register(t0);
RWTexture2D<float4> g_tPaint	: register(u0);

[numthreads(1, 1, 1)]
void ShowGIFCS( uint3 n3ThdID : SV_DispatchThreadID)
{
	if (0 == m_nFrame)
	{// 第一帧时用背景色擦除一下先
		g_tPaint[n3ThdID.xy] = m_c4BkColor;
	}

	if ( 2 == m_nDisposal )
	{//DM_BACKGROUND 用背景色填充整个画面，相当于一个Clear操作，因为命令列表中的ClearUAV命令只能设置一个float值，所以放到这里来清除背景
		g_tPaint[n3ThdID.xy] = m_c4BkColor;
	}
	else if (0 == m_nDisposal || 1 == m_nDisposal)
	{//DM_UNDEFINED or DM_NONE 不清楚背景，直接在原来画面基础上进行Alpha混色绘制
		// Simple Alpha Blending
		g_tPaint[n3ThdID.xy] = g_tGIFFrame[n3ThdID.xy].w * g_tGIFFrame[n3ThdID.xy]
			+ (1.0f - g_tGIFFrame[n3ThdID.xy].w) * g_tPaint[n3ThdID.xy];
	}
}