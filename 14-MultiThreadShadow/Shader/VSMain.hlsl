#include "Commons.hlsl"

cbuffer CB_SCENE_DATA : register(b0)
{
    float4x4 g_mxModel;
    float4x4 g_mxView;
    float4x4 g_mxProjection;
};

ST_PS_INPUT VSMain(float4 v4Position : POSITION, float4 v4Normal : NORMAL, float2 v2UV : TEXCOORD0, float4 v4Tangent : TANGENT)
{
    ST_PS_INPUT stOutput;

    float4 v4NewPosition = v4Position;

    v4NewPosition = mul(v4NewPosition, g_mxModel);

    stOutput.v4WorldPosition = v4NewPosition;

    v4NewPosition = mul(v4NewPosition, g_mxView);
    v4NewPosition = mul(v4NewPosition, g_mxProjection);

    stOutput.v4Position = v4NewPosition;
    stOutput.v2UV       = v2UV;
    stOutput.v4Normal   = v4Normal;
    stOutput.v4Normal.z *= -1.0f;
    stOutput.v4Tangent  = v4Tangent;

    return stOutput;
}
