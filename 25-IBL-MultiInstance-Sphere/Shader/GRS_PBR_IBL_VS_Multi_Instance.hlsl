// Multi-Instance PBR IBL Vertex Shader
#include "GRS_Scene_CB_Def.hlsli"

struct ST_GRS_HLSL_PBR_VS_INPUT
{
	float4		m_v4LPos		: POSITION;
	float4		m_v4LNormal		: NORMAL;
	float2		m_v2UV			: TEXCOORD;
	float4x4	m_mxModel2World	: WORLD;
	float4		m_v4Albedo		: COLOR0;    // 反射率
	float		m_fMetallic		: COLOR1;    // 金属度
	float		m_fRoughness	: COLOR2;    // 粗糙度
	float		m_fAO			: COLOR3;	 // 环境遮挡因子
	uint		m_nInstanceId	: SV_InstanceID;
};

struct ST_GRS_HLSL_PBR_PS_INPUT
{
	float4		m_v4HPos		: SV_POSITION;
	float4		m_v4WPos		: POSITION;
	float4		m_v4WNormal		: NORMAL;
	float2		m_v2UV			: TEXCOORD;
	float4x4	m_mxModel2World	: WORLD;
	float3		m_v3Albedo		: COLOR0;    // 反射率
	float		m_fMetallic		: COLOR1;    // 金属度
	float		m_fRoughness	: COLOR2;    // 粗糙度
	float		m_fAO			: COLOR3;    // 环境遮挡因子
};

ST_GRS_HLSL_PBR_PS_INPUT VSMain(ST_GRS_HLSL_PBR_VS_INPUT stVSInput)
{
	ST_GRS_HLSL_PBR_PS_INPUT stVSOutput;
	// 先按每个实例将物体变换到世界坐标系中
	stVSOutput.m_v4HPos = mul(stVSInput.m_v4LPos, stVSInput.m_mxModel2World);
	// 再进行世界坐标系整体变换,得到最终世界坐标系中的坐标
	stVSOutput.m_v4WPos = mul(stVSOutput.m_v4HPos, g_mxWorld);
	// 变换到视-投影空间中
	stVSOutput.m_v4HPos = mul(stVSOutput.m_v4HPos, g_mxWVP);

	// 向量先变换到世界坐标系中，再进行世界坐标变换
	stVSInput.m_v4LNormal.w = 0.0f;
	stVSOutput.m_v4WNormal = mul(stVSInput.m_v4LNormal, stVSInput.m_mxModel2World);
	stVSInput.m_v4LNormal.w = 0.0f;
	stVSOutput.m_v4WNormal = mul(stVSInput.m_v4LNormal, g_mxWorld);
	stVSInput.m_v4LNormal.w = 0.0f;
	stVSOutput.m_v4WNormal = normalize(stVSOutput.m_v4WNormal);

	stVSOutput.m_mxModel2World = stVSInput.m_mxModel2World;
	stVSOutput.m_v2UV = stVSInput.m_v2UV;
	stVSOutput.m_v3Albedo = stVSInput.m_v4Albedo.xyz;
	stVSOutput.m_fMetallic = stVSInput.m_fMetallic;
	stVSOutput.m_fRoughness = stVSInput.m_fRoughness;
	stVSOutput.m_fAO = stVSInput.m_fAO;
	return stVSOutput;
}