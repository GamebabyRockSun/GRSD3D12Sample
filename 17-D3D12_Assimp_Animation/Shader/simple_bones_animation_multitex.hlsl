// 02-3D_Animation_With_Assimp_D3D12 Vertex Shader
cbuffer cbMVP : register(b0)
{
    float4x4 mxWorld;                   //世界矩阵，这里其实是Model->World的转换矩阵
    float4x4 mxView;                    //视矩阵
    float4x4 mxProjection;              //投影矩阵
    float4x4 mxViewProj;                //视矩阵*投影
    float4x4 mxMVP;                     //世界*视矩阵*投影
};

cbuffer cbBones : register(b1)
{
    float4x4 mxBones[256];              //骨骼动画“调色板” 最多256根"大骨头"。内存大，显存多，任性！
};

struct VSInput
{
    float4 position : POSITION0;        //顶点位置
    float4 normal   : NORMAL0;          //法线
    float2 texuv    : TEXCOORD0;        //纹理坐标
    uint4  bonesID  : BLENDINDICES0;    //骨骼索引
    float4 fWeights : BLENDWEIGHT0;     //骨骼权重
};

struct PSInput
{
    float4 position : SV_POSITION0;        //位置
    float4 normal   : NORMAL0;          //法线
    float2 texuv    : TEXCOORD0;        //纹理坐标
};


PSInput VSMain(VSInput vin)
{
    PSInput vout;
    // 根据骨头索引以及权重计算骨头变换的复合矩阵
    float4x4 mxBonesTrans 
        = vin.fWeights[0] * mxBones[vin.bonesID[0]]
        + vin.fWeights[1] * mxBones[vin.bonesID[1]]
        + vin.fWeights[2] * mxBones[vin.bonesID[2]]
        + vin.fWeights[3] * mxBones[vin.bonesID[3]];

    // 变换骨头对顶点的影响
    vout.position = mul(vin.position, mxBonesTrans);
    
    //最终变换到视空间
    vout.position = mul(vout.position, mxMVP);   

    //vout.position = mul(vin.position, mxMVP);

    // 向量仅做世界矩阵变换
    vout.normal = mul(vin.normal, mxWorld);

    // 纹理坐标原样输出
    vout.texuv = vin.texuv;
    

    return vout;
}

Texture2D		g_txDiffuse : register(t0);
SamplerState    g_sampler   : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    //return float4( 0.9f, 0.9f, 0.9f, 1.0f );
    return float4(g_txDiffuse.Sample(g_sampler, input.texuv).rgb,1.0f);
}