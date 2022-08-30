
#define GRS_CUBE_MAP_FACE_CNT 6
#define GRS_LIGHT_COUNT 4

cbuffer CB_GRS_SCENE_MATRIX : register(b0)
{
    matrix g_mxWorld;
    matrix g_mxView;
    matrix g_mxProj;
    matrix g_mxWorldView;
    matrix g_mxWVP;
    matrix g_mxInvWVP;
    matrix g_mxVP;
    matrix g_mxInvVP;
};

cbuffer CB_GRS_GS_VIEW_MATRIX : register(b1)
{
    matrix g_mxGSCubeView[GRS_CUBE_MAP_FACE_CNT]; // View matrices for cube map rendering
};

// Camera
cbuffer CB_GRS_SCENE_CAMERA : register(b2)
{
    float4 g_v4EyePos;
    float4 g_v4LookAt;
    float4 g_v4UpDir ;
}
// lights
cbuffer CB_GRS_SCENE_LIGHTS : register(b3)
{
    float4 g_v4LightPos[GRS_LIGHT_COUNT];
    float4 g_v4LightColors[GRS_LIGHT_COUNT];
};

// Material
cbuffer CB_GRS_PBR_MATERIAL : register(b4)
{
    float  g_fMetallic;   // 金属度
    float  g_fRoughness;  // 粗糙度
    float  g_fAO;		  // 环境遮挡系数
    float4 g_v4Albedo;    // 反射率
};

