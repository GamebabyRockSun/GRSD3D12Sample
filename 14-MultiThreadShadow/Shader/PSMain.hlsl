
Texture2D g_t2dDiffuseMap : register(t0);
Texture2D g_t2dNormalMap : register(t1);
Texture2D g_t2dShadowMap : register(t2);

SamplerState g_SampleWrap : register(s0);
SamplerState g_SampleClamp : register(s1);

#define GRS_NUM_LIGHTS 3
#define GRS_SHADOW_DEPTH_BIAS 0.0005f

struct LightState
{
    float4 v4Position;
    float4 v4Direction;
    float4 v4Color;
    float4 v4Falloff;
    float4x4 mxView;
    float4x4 mxProjection;
};

cbuffer LightBuffer : register(b1)
{
    float4 v4AmbientColor;
    LightState stLights[GRS_NUM_LIGHTS];
    uint bIsSampleShadowMap;
};

struct PSInput
{
    float4 v4Position       : SV_POSITION;
    float4 v4WorldPosition  : POSITION;
    float2 v2UV             : TEXCOORD0;
    float4 v4Normal         : NORMAL;
    float4 v4Tangent        : TANGENT;
};

//--------------------------------------------------------------------------------------
// 采样法线贴图，转换为有符号法线，并从切线空间变换到世界空间。
//--------------------------------------------------------------------------------------
float3 CalcPerPixelNormal(float2 vTexcoord, float3 vVertNormal, float3 vVertTangent)
{
    vVertNormal = normalize(vVertNormal);
    vVertTangent = normalize(vVertTangent);

    float3 vVertBinormal = normalize(cross(vVertNormal,vVertTangent));
    // 生成切空间变换矩阵（逆矩阵就是从世界空间变换到切线空间）
    float3x3 mTangentSpaceToWorldSpace = float3x3(vVertTangent, vVertNormal,  vVertBinormal);

    mTangentSpaceToWorldSpace = transpose(mTangentSpaceToWorldSpace);

    // 采样逐像素法线
    float3 vBumpNormal = (float3)g_t2dNormalMap.Sample(g_SampleWrap, vTexcoord);
    // 符号化还原
    vBumpNormal = 2.0f * vBumpNormal - 1.0f;
    vBumpNormal.z = -vBumpNormal.z;

    return mul(vBumpNormal, mTangentSpaceToWorldSpace);
}

//--------------------------------------------------------------------------------------
// 漫射光照计算，使用角度与距离衰减。
//--------------------------------------------------------------------------------------
float4 CalcLightingColor(float3 vLightPos, float3 vLightDir, float4 vLightColor, float4 vFalloffs, float3 vPosWorld, float3 vPerPixelNormal)
{
    float3 vLightToPixelUnNormalized = vPosWorld - vLightPos;

    // Dist v4Falloff = 0 at vFalloffs.x, 1 at vFalloffs.x - vFalloffs.y
    float fDist = length(vLightToPixelUnNormalized);

    float fDistFalloff = saturate((vFalloffs.x - fDist) / vFalloffs.y);

    // Normalize from here on.
    float3 vLightToPixelNormalized = vLightToPixelUnNormalized / fDist;

    // Angle v4Falloff = 0 at vFalloffs.z, 1 at vFalloffs.z - vFalloffs.w
    float fCosAngle = dot(vLightToPixelNormalized, vLightDir / length(vLightDir));
    float fAngleFalloff = saturate((fCosAngle - vFalloffs.z) / vFalloffs.w);

    // Diffuse contribution.
    float fNDotL = saturate(-dot(vLightToPixelNormalized, vPerPixelNormal));

    return vLightColor * fNDotL * fDistFalloff * fAngleFalloff;
}

//--------------------------------------------------------------------------------------
// 测试有多少像素是在阴影中，使用2x2百分比接近过滤。
//--------------------------------------------------------------------------------------
float4 CalcUnshadowedAmountPCF2x2(int lightIndex, float4 vPosWorld)
{
    // Compute pixel v4Position in light space.
    float4 vLightSpacePos = vPosWorld;
    vLightSpacePos = mul(vLightSpacePos, stLights[lightIndex].mxView);
    vLightSpacePos = mul(vLightSpacePos, stLights[lightIndex].mxProjection);

    vLightSpacePos.xyz /= vLightSpacePos.w;

    // Translate from homogeneous coords to texture coords.
    float2 vShadowTexCoord = 0.5f * vLightSpacePos.xy + 0.5f;
    vShadowTexCoord.y = 1.0f - vShadowTexCoord.y;

    // Depth bias to avoid pixel self-shadowing.
    float vLightSpaceDepth = vLightSpacePos.z - GRS_SHADOW_DEPTH_BIAS;

    // Find sub-pixel weights.
    
    float2 vShadowMapDims = float2(1280.0f, 720.0f); // need to keep in sync with .cpp file
    g_t2dShadowMap.GetDimensions(vShadowMapDims.x, vShadowMapDims.y);

    float4 vSubPixelCoords = float4(1.0f, 1.0f, 1.0f, 1.0f);
    vSubPixelCoords.xy = frac(vShadowMapDims * vShadowTexCoord);
    vSubPixelCoords.zw = 1.0f - vSubPixelCoords.xy;
    float4 vBilinearWeights = vSubPixelCoords.zxzx * vSubPixelCoords.wwyy;

    // 2x2 percentage closer filtering.
    float2 vTexelUnits = 1.0f / vShadowMapDims;
    float4 vShadowDepths;
    vShadowDepths.x = g_t2dShadowMap.Sample(g_SampleClamp, vShadowTexCoord);
    vShadowDepths.y = g_t2dShadowMap.Sample(g_SampleClamp, vShadowTexCoord + float2(vTexelUnits.x, 0.0f));
    vShadowDepths.z = g_t2dShadowMap.Sample(g_SampleClamp, vShadowTexCoord + float2(0.0f, vTexelUnits.y));
    vShadowDepths.w = g_t2dShadowMap.Sample(g_SampleClamp, vShadowTexCoord + vTexelUnits);

    // What weighted fraction of the 4 samples are nearer to the light than this pixel?
    float4 vShadowTests = (vShadowDepths >= vLightSpaceDepth) ? 1.0f : 0.0f;
    return dot(vBilinearWeights, vShadowTests);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 diffuseColor = g_t2dDiffuseMap.Sample(g_SampleWrap, input.v2UV);
    float3 pixelNormal = CalcPerPixelNormal(input.v2UV, input.v4Normal, input.v4Tangent);
    float4 totalLight = v4AmbientColor;

    for (int i = 0; i < GRS_NUM_LIGHTS; i++)
    {
        float4 lightPass = CalcLightingColor(stLights[i].v4Position.xyz
            , stLights[i].v4Direction.xyz
            , stLights[i].v4Color
            , stLights[i].v4Falloff
            , input.v4WorldPosition.xyz
            , pixelNormal.xyz);
        if ( 1 == bIsSampleShadowMap && i == 0 )
        {
            lightPass *= CalcUnshadowedAmountPCF2x2(i, input.v4WorldPosition);
        }
        totalLight += lightPass;
    }

    //return float4(pixelNormal, 1.0f);

    float4 vLightSpacePos = input.v4WorldPosition;
    vLightSpacePos = mul(vLightSpacePos, stLights[0].mxView);
    vLightSpacePos = mul(vLightSpacePos, stLights[0].mxProjection);
    vLightSpacePos.xyz /= vLightSpacePos.w;

    float2 v2LightCoord = 0.5f * vLightSpacePos.xy + 0.5f;
    v2LightCoord.y = 1.0f - v2LightCoord.y;
    float4 fPixelColor = diffuseColor;
    float fDepthValue = g_t2dShadowMap.Sample(g_SampleClamp, v2LightCoord);
    vLightSpacePos.z -= 0.05f;

    if ( vLightSpacePos.z < fDepthValue )
    {
        fPixelColor += v4AmbientColor;
        //float lightIntensity = saturate(-dot(pixelNormal.xyz, stLights[0].v4Direction.xyz));
        //if ( lightIntensity > 0.0f )
        //{
    
        //    fPixelColor += (diffuseColor * lightIntensity);
        //}
    }
    else
    {
        fPixelColor -= v4AmbientColor;
    }
    return fPixelColor;
    //return float4(vLightSpacePos.z, vLightSpacePos.z, vLightSpacePos.z, 1.0f);

    //return vLightSpacePos;
    //
    //return CalcUnshadowedAmountPCF2x2(0, input.v4WorldPosition);
    
    //float d = g_t2dShadowMap.Sample(g_SampleClamp, input.v2UV);
    //return float4(d, d, d, 1.0f);
    //return (pixelNormal.xyz/length(pixelNormal.xyz),1.0f);
       
    return diffuseColor * saturate(totalLight);
}
