// Simplified PBR HLSL Vertex Shader

cbuffer MVPBuffer : register( b0 )
{
	float4x4 mxWorld;                   //世界矩阵，这里其实是Model->World的转换矩阵
	float4x4 mxView;                    //视矩阵
	float4x4 mxProjection;				//投影矩阵
	float4x4 mxViewProj;                //视矩阵*投影
	float4x4 mxMVP;                     //世界*视矩阵*投影
};

struct PSInput
{
	float4 m_v4PosWVP : SV_POSITION;
	float4 m_v4PosWorld : POSITION;
	float4 m_v4Normal: NORMAL;
	float2 m_v2UV: TEXCOORD;
};

PSInput VSMain( float4 v4LocalPos : POSITION, float4 v4LocalNormal:NORMAL, float2 v2UV : TEXCOORD )
{
	PSInput stOutput;

	stOutput.m_v4PosWVP = mul( v4LocalPos, mxMVP );
	stOutput.m_v4PosWorld = mul( v4LocalPos, mxWorld);
	v4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = mul( v4LocalNormal, mxWorld);
	stOutput.m_v2UV = v2UV;
	return stOutput;
}