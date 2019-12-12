struct PSInput
{
	float4 m_v4Position : SV_POSITION;
	float2 m_v2UV : TEXCOORD;

};

cbuffer MVPBuffer : register(b0)
{
	float4x4 m_MVP;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 v4Position : POSITION, float2 v2UV : TEXCOORD,float3 v3Normal:NORMAL)
{
	PSInput stResult;
	stResult.m_v4Position = mul(v4Position, m_MVP);
	stResult.m_v2UV = v2UV;

	return stResult;
}

float4 PSMain(PSInput stPSInput) : SV_TARGET
{
	return g_texture.Sample(g_sampler, stPSInput.m_v2UV);
}
