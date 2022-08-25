
cbuffer ST_CB_SCENE : register(b0)
{
    float4   g_v4LightPos;
    float4x4 g_mxModel;
    float4x4 g_mxView;
    float4x4 g_mxProjection;
    float4x4 g_mxVPLight;
};

Texture2D g_t2dDiffuseMap : register(t0);
Texture2D g_t2dNormalMap : register(t1);
Texture2D g_t2dShadowMap : register(t2);

SamplerState g_SampleWrap : register(s0);
SamplerState g_SampleClamp : register(s1);

struct ST_PS_INPUT
{
    float4 m_v4Position             : SV_POSITION;
    float4 m_v4WorldPosition        : POSITION0;
    float4 m_v4LightSpacePosition   : POSITION1;
    float2 m_v2UV                   : TEXCOORD0;
    float4 m_v4Normal               : NORMAL;
    float4 m_v4Tangent              : TANGENT;
};

ST_PS_INPUT VSMain(float4 v4Position : POSITION
    , float4 v4Normal : NORMAL
    , float2 v2UV : TEXCOORD0
    , float4 v4Tangent : TANGENT)
{
    ST_PS_INPUT stOutput;

    float4 v4NewPosition = v4Position;

    v4NewPosition = mul(v4NewPosition, g_mxModel);

    stOutput.m_v4WorldPosition = v4NewPosition;

    stOutput.m_v4LightSpacePosition = mul(v4NewPosition, g_mxVPLight);

    v4NewPosition = mul(v4NewPosition, g_mxView);
    v4NewPosition = mul(v4NewPosition, g_mxProjection);

    stOutput.m_v4Position = v4NewPosition;
    stOutput.m_v2UV = v2UV;
    stOutput.m_v4Normal = v4Normal;
    stOutput.m_v4Normal.z *= -1.0f;
    stOutput.m_v4Normal.w = 0.0f;
    stOutput.m_v4Normal = normalize(stOutput.m_v4Normal);
    stOutput.m_v4Tangent = v4Tangent;

    return stOutput;
}

float ShadowCalculation(float4 fragPosLightSpace)
{
    // 执行透视除法
    float4 projCoords = fragPosLightSpace;
    projCoords.xyz /= projCoords.w;
    // 变换到[0,1]的范围
    projCoords = projCoords * 0.5 + 0.5;
    // 取得最近点的深度(使用[0,1]范围下的fragPosLight当坐标)
    float closestDepth = g_t2dShadowMap.Sample(g_SampleClamp, projCoords.xy).r;
    // 取得当前片段在光源视角下的深度
    float currentDepth = projCoords.z;
    // 检查当前片段是否在阴影中
    float fInShadow = currentDepth > closestDepth ? 1.0f : 0.0f;

    return fInShadow;
}

float4 PSMain(ST_PS_INPUT stPSin) : SV_TARGET
{
    float3 v3TexelColor = g_t2dDiffuseMap.Sample(g_SampleWrap,stPSin.m_v2UV).rgb;
    float3 v3Normal = stPSin.m_v4Normal.xyz;

    float3 v3LightColor = 1.0f;
    // Ambient
    float3 v3AmbientColor = 0.15f * v3TexelColor;
    // Diffuse
    float3 v3LightDir = normalize(g_v4LightPos.xyz - stPSin.m_v4WorldPosition.xyz);
    float  fDiff = max(dot(v3LightDir, v3Normal), 0.0);
    float3 v3DiffuseColor = fDiff * v3LightColor;
    // Specular
    float3 v3ViewDir = normalize(viewPos - fs_in.FragPos);
    float3 v3ReflectDir = reflect(-v3LightDir, v3Normal);
    float3 v3HalfwayDir = normalize(v3LightDir + v3ViewDir);
    float fSpec = pow(max(dot(v3Normal, v3HalfwayDir), 0.0), 64.0);
    float3 v3SpecularColor = fSpec * v3LightColor;
    // 计算阴影
    float fInShadow = ShadowCalculation(stPSin.m_v4LightSpacePosition);
    float3 v3PixelColor = (v3AmbientColor + (1.0 - fInShadow) * (v3DiffuseColor + v3SpecularColor)) * v3TexelColor;

    return (v3PixelColor.rgb, 1.0f);
}
