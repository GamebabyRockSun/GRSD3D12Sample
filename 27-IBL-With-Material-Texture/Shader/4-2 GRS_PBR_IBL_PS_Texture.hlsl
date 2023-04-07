#include "0-3 GRS_Scene_CB_Def.hlsli"
#include "0-2 GRS_PBR_Function.hlsli"
#include "0-1 HDR_COLOR_CONV.hlsli"

SamplerState g_sapLinear		    : register(s0);

// IBL Lights
TextureCube  g_texSpecularCubemap   : register(t0, space0); // 镜面反射
TextureCube  g_texDiffuseCubemap    : register(t1, space0); // 漫反射光照
Texture2D    g_texLut               : register(t2, space0); // BRDF LUT

// PBR Material
Texture2D    g_texAlbedo            : register(t0, space1); // 基础反射率（Base Color or Albedo)
Texture2D    g_texNormal            : register(t1, space1); // 法线
Texture2D    g_texDisplacement      : register(t2, space1); // 位移贴图(高度图)
Texture2D    g_texMetalness         : register(t3, space1); // 金属度
Texture2D    g_texRoughness         : register(t4, space1); // 粗糙度
Texture2D    g_texAO                : register(t5, space1); // AO

struct ST_GRS_HLSL_PBR_PS_INPUT
{
    // Per Vertex Data
    float4		m_v4HPos		: SV_POSITION;
    float4		m_v4WPos		: POSITION;
    float4		m_v4WNormal		: NORMAL;
    float2		m_v2UV			: TEXCOORD;
};

float3 CalcNewNormal(float2 v2UV, float3 v3Pos, float3 v3OldNormal/*ST_GRS_HLSL_VS_OUTPUT stPSInput*/)
{
    float fDisplacementScale = 0.1f;
    // 水平方向uv的增量
    float2 v2UVdx = ddx_fine(v2UV);
    // 垂直方向uv的增量
    float2 v2UVdy = ddy_fine(v2UV);
    // 当前置换距离
    float fHeight = g_texDisplacement.Sample(g_sapLinear, v2UV).r;
    // 水平方向下一个像素点的置换距离
    float fHx = g_texDisplacement.Sample(g_sapLinear, v2UV + v2UVdx).r;
    // 垂直方向下一个像素点的置换距离
    float fHy = g_texDisplacement.Sample(g_sapLinear, v2UV + v2UVdy).r;
    // 水平方向置换增量
    float fHdx = (fHx - fHeight) * fDisplacementScale;
    // 垂直方向置换增量
    float fHdy = (fHy - fHeight) * fDisplacementScale;

    // 水平方向顶点坐标增量，作为切线
    float3 v3Tangent = ddx_fine(v3Pos.xyz);
    // 垂直方向顶点坐标增量，作为副切线
    float3 v3BinTangent = ddy_fine(v3Pos.xyz);

    // 到这里为止，其实已经可以用 v3Tangent 差乘 v3BinTangent 来得到 v3NewNormal 了
    // 但是会发现 v3NewNormal 并不平滑，用来计算光照会出现硬边
    // 解决办法是使用从 vertex shader 传过来的 normal 来进行纠正，顶点上的 normal 是平滑的

    // corss 部分就是用平滑的 normal 来重新计算新的 v3Tangent
    // 后面加法是使用置换增量来对其进行扰动
    float3 v3NewTangent = cross(v3BinTangent, v3OldNormal) + v3OldNormal * fHdx;
    // 同理
    float3 v3NewBinTangent = cross(v3OldNormal, v3Tangent) + v3OldNormal * fHdy;
    // 最终得到平滑的 v3NewNormal
    float3 v3NewNormal = cross(v3NewTangent, v3NewBinTangent);
    v3NewNormal = normalize(v3NewNormal);

    // 这里就可以使用这个 v3NewNormal 参与光照计算了
    return v3NewNormal;
}

float4 PSMain(ST_GRS_HLSL_PBR_PS_INPUT stPSInput) : SV_TARGET
{
    float3 v3Albedo = g_texAlbedo.Sample(g_sapLinear, stPSInput.m_v2UV).rgb;
    float3 N = g_texNormal.Sample(g_sapLinear, stPSInput.m_v2UV).xyz;
    float  fMetallic = g_texMetalness.Sample(g_sapLinear, stPSInput.m_v2UV).r;
    float  fRoughness = g_texRoughness.Sample(g_sapLinear, stPSInput.m_v2UV).r;
    float  fAO = g_texAO.Sample(g_sapLinear, stPSInput.m_v2UV).r;

    // 法线贴图中取得的法线的标准解算
    N = 2.0f * N - 1.0f;
    N = normalize(N);

    // 计算切空间
    float3 up = float3(0.0f, 1.0f, 0.0f);
    if (dot(up, stPSInput.m_v4WNormal.xyz) < 1e-6)
    {
        up = float3(1.0f, 0.0f, 0.0f);
    }
    float3 right = normalize(cross(up, stPSInput.m_v4WNormal.xyz));
    up = cross(stPSInput.m_v4WNormal.xyz, right);

    // 重新计算逐像素法线
    N = right * N.x + up * N.y + stPSInput.m_v4WNormal.xyz * N.z;

    //N = CalcNewNormal(stPSInput.m_v2UV, stPSInput.m_v4WPos.xyz, N);

    //return float4(N, 1.0f);
    //return float4(v3Albedo, 1.0f);
    //return float4(fMetallic, 0.0f, 0.0f, 1.0f);
    //return float4(fRoughness, 0.0f, 0.0f, 1.0f);
    //return float4(fAO, 0.0f, 0.0f, 1.0f);

    // 颜色转换到线性空间（纹理颜色空间本身就是线性空间时，不需要这个转换）
    //v3Albedo = SRGBToLinear(v3Albedo);

    float3 V = normalize(g_v4EyePos.xyz - stPSInput.m_v4WPos.xyz); // 视向量
    float3 R = reflect(-V, N);    // 视向量的镜面反射向量

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, v3Albedo, fMetallic);

    float3 Lo = float3(0.0f,0.0f,0.0f);

    float NdotV = max(dot(N, V), 0.0f);

    // 首先按照之前的样子计算直接光照的效果
    for (int i = 0; i < GRS_LIGHT_COUNT; ++i)
    {
        // 计算点光源的照射
        // 因为是多实例渲染，所以对每个光源做下位置变换，这样相当于每个实例有了自己的光源
        // 注意这不是标准操作，一定要注意正式的代码中不要这样做
        float4 v4LightWorldPos = mul(g_v4LightPos[i], g_mxWorld);

        float3 L = v4LightWorldPos.xyz - stPSInput.m_v4WPos.xyz;       // 入射光线
        float  distance = length(L);
        float  attenuation = 1.0 / (distance * distance);
        float3 radiance = g_v4LightColors[i].rgb * attenuation; // 距离平方反比衰减

        L = normalize(L);

        float3 H = normalize(V + L);    // 视向量与光线向量的中间向量

        float  NdotL = max(dot(N, L), 0.0f);

        // Cook-Torrance BRDF
        float   NDF = DistributionGGX(N, H, fRoughness);
        float   G = GeometrySmith_DirectLight(NdotV ,NdotL , fRoughness);
        float3  F = FresnelSchlick(NdotV , F0);

        float3 numerator = NDF * G * F;
        float  denominator = 4.0 * NdotV * NdotL + 0.0001; // + 0.0001 to prevent divide by zero
        float3 specular = numerator / denominator;

        float3 kS = F;
        float3 kD = float3(1.0f,1.0f,1.0f) - kS;
        kD *= 1.0 - fMetallic;

        Lo += (kD * v3Albedo / PI + specular) * radiance * NdotL; // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }

    // 接着开始利用前面的预积分结果计算IBL光照的效果

    // 根据粗糙度插值计算菲涅尔系数（与直接光照的菲涅尔系数计算方法不同）
    float3 F = FresnelSchlickRoughness(NdotV, F0, fRoughness);

    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= (1.0 - fMetallic); // 简单但非常重要的计算步骤，表达了纯金属是没有漫反射的

    // IBL漫反射环境光部分
    float3 irradiance = g_texDiffuseCubemap.Sample(g_sapLinear, N).rgb;
    float3 diffuse = irradiance * v3Albedo;

    float fCubeWidth = 0.0f;
    float fCubeHeight = 0.0f;
    float fCubeMapLevels = 0.0f;
    // 获取 CubeMap 的 Max Maplevels 
    g_texSpecularCubemap.GetDimensions(0, fCubeWidth, fCubeHeight, fCubeMapLevels);
    // IBL镜面反射环境光部分
    float3 prefilteredColor = g_texSpecularCubemap.SampleLevel(g_sapLinear, R, fRoughness * fCubeMapLevels).rgb;
    float2 brdf = g_texLut.Sample(g_sapLinear, float2(NdotV, fRoughness)).rg;
    float3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);

    // IBL 光照合成
    float3 ambient = (kD * diffuse + specular) * fAO;

    // 直接光照 + IBL光照
    float3 color = ambient + Lo;

    //return float4(color, 1.0f);

    // Gamma
    return float4(LinearToSRGB(color),1.0f);
}