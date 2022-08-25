cbuffer ST_CB_LIGHT_MATRIX : register(b0)
{
    float4x4 g_mxModel;
    float4x4 g_mxLightView;
    float4x4 g_mxLightProjection;
};

struct ST_PS_INPUT
{
    float4 m_v4Position : SV_POSITION;
};

ST_PS_INPUT VSMain(float4 v4Position : POSITION)
{
    ST_PS_INPUT stOutput;

    stOutPut.m_v4Position = mul(v4Position, g_mxModel);
    stOutPut.m_v4Position = mul(stOutPut.m_v4Position, g_mxLightView);
    stOutPut.m_v4Position = mul(stOutPut.m_v4Position, g_mxLightProjection);

    return stOutput;
}
