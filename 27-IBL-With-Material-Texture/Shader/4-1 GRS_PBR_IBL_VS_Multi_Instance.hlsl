// Multi-Instance PBR IBL Vertex Shader
#include "0-3 GRS_Scene_CB_Def.hlsli"

struct ST_GRS_HLSL_PBR_VS_INPUT
{
	float4		m_v4LPos		: POSITION;
	float4		m_v4LNormal		: NORMAL;
	float2		m_v2UV			: TEXCOORD;
};

struct ST_GRS_HLSL_PBR_PS_INPUT
{
	float4		m_v4HPos		: SV_POSITION;
	float4		m_v4WPos		: POSITION;
	float4		m_v4WNormal		: NORMAL;
	float2		m_v2UV			: TEXCOORD;
};

SamplerState g_sapLinear		    : register(s0);
Texture2D    g_texDisplacement      : register(t2, space1); // 位移贴图(高度图)

ST_GRS_HLSL_PBR_PS_INPUT VSMain(ST_GRS_HLSL_PBR_VS_INPUT stVSInput)
{
	ST_GRS_HLSL_PBR_PS_INPUT stVSOutput;

	// 根据位移贴图直接改变物体外形，更好的做法是使用 Tesselation
	float fDis = (g_texDisplacement.SampleLevel(g_sapLinear, stVSInput.m_v2UV, 0.0f).r * 0.1f);
	stVSInput.m_v4LPos += fDis * normalize(stVSInput.m_v4LNormal);

	// 局部坐标转换到世界空间和裁剪空间
	stVSOutput.m_v4WPos = mul(stVSInput.m_v4LPos, g_mxModel2World);
	stVSOutput.m_v4WPos = mul(stVSOutput.m_v4WPos, g_mxWorld);
	stVSOutput.m_v4HPos = mul(stVSOutput.m_v4WPos, g_mxVP);

	// 法线先变换到世界坐标系中，再进行世界坐标变换;
	stVSInput.m_v4LNormal.w = 0.0f;
	stVSOutput.m_v4WNormal = mul(stVSInput.m_v4LNormal, g_mxModel2World);
	stVSOutput.m_v4WNormal = normalize(mul(stVSOutput.m_v4WNormal, g_mxWorld));
	stVSOutput.m_v2UV = stVSInput.m_v2UV;

	return stVSOutput;
}