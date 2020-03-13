struct PSInput
{
	float4 m_v4Pos	: SV_POSITION;
	float2 m_v2UV	: TEXCOORD0;
};

PSInput VSMain(float4 v4Pos : POSITION, float2 v2UV : TEXCOORD0)
{
	PSInput stResult;
	stResult.m_v4Pos =v4Pos;
	stResult.m_v2UV = v2UV;

	return stResult;
}