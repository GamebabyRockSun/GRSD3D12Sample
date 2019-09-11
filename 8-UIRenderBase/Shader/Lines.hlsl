struct PSInput
{
	float4 position : SV_POSITION;
	float4 color :COLOR;
	float2 uv : TEXCOORD;
};

cbuffer MVOBuffer : register(b0)
{
	float4x4 m_MVO;
};


PSInput VSMain(float4 position : POSITION, float4 color : COLOR,float2 uv : TEXCOORD)
{
	PSInput result;
	result.position = mul(position, m_MVO);
	result.color = color;
	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return input.color;
}
