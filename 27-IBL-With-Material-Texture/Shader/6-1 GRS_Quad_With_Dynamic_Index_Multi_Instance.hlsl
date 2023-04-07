#include "0-1 HDR_COLOR_CONV.hlsli"
#include "0-3 GRS_Scene_CB_Def.hlsli"

Texture2D g_Texture2DArray[] : register(t0);
SamplerState g_smpLinear : register(s0);

struct ST_GRS_HLSL_VS_IN
{
	float4	 m_v4LPosition	 : POSITION;
	float4	 m_v4Color		 : COLOR0;
	float2	 m_v2UV			 : TEXCOORD;
	float4x4 m_mxQuad2World	 : WORLD;
	uint	 m_nTextureIndex : COLOR1;
};

struct ST_GRS_HLSL_PS_IN
{
	float4		m_v4Position	: SV_POSITION;
	float4		m_v4Color		: COLOR0;
	float2		m_v2UV			: TEXCOORD;
	uint		m_nTextureIndex : COLOR1;
};

ST_GRS_HLSL_PS_IN VSMain( ST_GRS_HLSL_VS_IN stVSInput )
{
	ST_GRS_HLSL_PS_IN stVSOutput;

	stVSOutput.m_v4Position = mul(stVSInput.m_v4LPosition, stVSInput.m_mxQuad2World);

	// 下面两句是二选一，一般2D渲染的时候 World 和 View 矩阵都取了单位矩阵，但不排除特殊情况，所以一般还是乘以MVP综合矩阵
	//stVSOutput.m_v4Position = mul(stVSOutput.m_v4Position, g_mxProj);
	stVSOutput.m_v4Position		= mul(stVSOutput.m_v4Position, g_mxWVP);
	
	stVSOutput.m_v4Color		= stVSInput.m_v4Color;
	stVSOutput.m_v2UV			= stVSInput.m_v2UV;	
	stVSOutput.m_nTextureIndex	= stVSInput.m_nTextureIndex;

	return stVSOutput;
}

float4 PSMain(ST_GRS_HLSL_PS_IN stPSInput) : SV_TARGET
{
	//float3 v3SRGBColor 
	//= LinearToSRGB(g_Texture2DArray[stPSInput.m_nTextureIndex].Sample(g_smpLinear, stPSInput.m_v2UV).rgb);
	//return float4(v3SRGBColor,1.0f);

	return float4(g_Texture2DArray[stPSInput.m_nTextureIndex].Sample(g_smpLinear, stPSInput.m_v2UV).rgb,1.0f);
}
