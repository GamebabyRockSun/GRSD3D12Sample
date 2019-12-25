struct PSInput
{
    float4 v4Position       : SV_POSITION;
    float4 v4WorldPosition  : POSITION;
    float2 v2UV             : TEXCOORD0;
    float4 v4Normal         : NORMAL;
    float4 v4Tangent        : TANGENT;
};