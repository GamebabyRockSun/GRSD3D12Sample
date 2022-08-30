
cbuffer CB_GRS_SCENE_MATRIX : register(b0)
{
    matrix g_mxModel;           // Model self
    matrix g_mxInvModel;        // Inverse Model
    matrix g_mxModel2World;     // Model -> World (Sacle -> Rotate -> Displacement;
    matrix g_mxInvModel2World;  // Inverse Model -> World <=> World -> Model
    matrix g_mxWorld;           // World self
    matrix g_mxView;            // View self
    matrix g_mxProj;            // Proj self
    matrix g_mxWorldView;       // World * View
    matrix g_mxWVP;             // World * View * Proj
    matrix g_mxInvWVP;          // Inverse WVP
    matrix g_mxVP;              // View * Proj
    matrix g_mxInvVP;           // Inverse VP
};

// Camera
cbuffer CB_GRS_SCENE_CAMERA : register(b1)
{
    float4 g_v4EyePos;
    float4 g_v4LookAt;
    float4 g_v4UpDir;
}
// lights
cbuffer CB_GRS_SCENE_LIGHTS : register(b2)
{
    float4 g_v4LightPos;
    float4 g_v4LightColors;
};