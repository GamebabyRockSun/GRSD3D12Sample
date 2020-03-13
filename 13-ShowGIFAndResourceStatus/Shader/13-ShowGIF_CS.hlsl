
cbuffer ST_GRS_GIF_FRAME_PARAM : register(b0)
{
	float4	m_c4BkColor;		
	uint2	m_nLeftTop;
	uint2	m_nFrameWH;
	uint	m_nFrame;
	uint	m_nDisposal;
};

// 帧纹理，尺寸就是当前帧画面的大小，注意这个往往不是整个GIF的大小
Texture2D			g_tGIFFrame : register(t0);

// 最终代表整个GIF画板纹理，大小就是整个GIF的大小
RWTexture2D<float4> g_tPaint	: register(u0);

[numthreads(1, 1, 1)]
void ShowGIFCS( uint3 n3ThdID : SV_DispatchThreadID)
{
	if ( 0 == m_nFrame )
	{// 第一帧时用背景色整个绘制一下先，相当于一个Clear操作
		g_tPaint[n3ThdID.xy] = m_c4BkColor;
	}

	// 注意画板的像素坐标需要偏移，画板坐标是：m_nLeftTop.xy + n3ThdID.xy
	// m_nLeftTop.xy就是当前帧相对于整个画板左上角的偏移值
	// n3ThdID.xy就是当前帧中对应要绘制的像素点坐标

	if ( 0 == m_nDisposal )
	{//DM_NONE 不清理背景
		g_tPaint[ m_nLeftTop.xy + n3ThdID.xy ] = g_tGIFFrame[n3ThdID.xy];
	}
	else if (1 == m_nDisposal)
	{//DM_UNDEFINED 直接在原来画面基础上进行Alpha混色绘制
		// 注意g_tGIFFrame[n3ThdID.xy].w只是简单的0或1值，所以没必要进行真正的Alpha混色计算
		// 但这里也没有使用IF Else判断，而是用了开销更小的?:三元表达式来进行计算
		g_tPaint[ m_nLeftTop.xy + n3ThdID.xy ] 
			= g_tGIFFrame[n3ThdID.xy].w ? g_tGIFFrame[n3ThdID.xy] : g_tPaint[m_nLeftTop.xy + n3ThdID.xy];
	}
	else if ( 2 == m_nDisposal )
	{// DM_BACKGROUND 用背景色填充画板，然后绘制，相当于用背景色进行Alpha混色，过程同上
		g_tPaint[m_nLeftTop.xy + n3ThdID.xy] = g_tGIFFrame[n3ThdID.xy].w ? g_tGIFFrame[n3ThdID.xy] : m_c4BkColor;
	}

	else if( 3 == m_nDisposal )
	{// DM_PREVIOUS 保留前一帧
		g_tPaint[ m_nLeftTop.xy + n3ThdID.xy ] = g_tPaint[n3ThdID.xy + m_nLeftTop.xy];
	}
}