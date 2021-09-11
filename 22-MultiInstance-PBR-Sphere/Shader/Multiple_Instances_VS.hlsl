// Simplified PBR HLSL Vertex Shader

cbuffer MVPBuffer : register( b0 )
{
	float4x4 mxWorld;                   //世界矩阵，这里其实是Model->World的转换矩阵
	float4x4 mxView;                    //视矩阵
	float4x4 mxProjection;				//投影矩阵
	float4x4 mxViewProj;                //视矩阵*投影
	float4x4 mxMVP;                     //世界*视矩阵*投影
};

struct VSInput
{
	float4		mv4LocalPos		: POSITION;
	float4		mv4LocalNormal	: NORMAL;
	float2		mv2UV			: TEXCOORD;
	float4x4	mxModel2World	: WORLD;
	float4		mv4Albedo		: COLOR0;    // 反射率
	float		mfMetallic		: COLOR1;    // 金属度
	float		mfRoughness		: COLOR2;    // 粗糙度
	float		mfAO			: COLOR3;
	uint		mnInstanceId	: SV_InstanceID;
};

struct PSInput
{
	float4		m_v4PosWVP		: SV_POSITION;
	float4		m_v4PosWorld	: POSITION;
	float4		m_v4Normal		: NORMAL;
	float2		m_v2UV			: TEXCOORD;
	float4x4	mxModel2World	: WORLD;
	float3		mv3Albedo		: COLOR0;    // 反射率
	float		mfMetallic		: COLOR1;    // 金属度
	float		mfRoughness		: COLOR2;    // 粗糙度
	float		mfAO			: COLOR3;
};

PSInput VSMain( VSInput vsInput )
{
	PSInput stOutput;
	// 先按每个实例将物体变换到世界坐标系中
	stOutput.m_v4PosWVP = mul( vsInput.mv4LocalPos, vsInput.mxModel2World );
	// 再进行世界坐标系整体变换,得到最终世界坐标系中的坐标
	stOutput.m_v4PosWorld = mul( stOutput.m_v4PosWVP, mxWorld );
	// 变换到视-投影空间中
	stOutput.m_v4PosWVP = mul( stOutput.m_v4PosWVP, mxMVP );

	// 向量先变换到世界坐标系中，再进行世界坐标变换
	vsInput.mv4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = mul( vsInput.mv4LocalNormal, vsInput.mxModel2World );
	vsInput.mv4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = mul( vsInput.mv4LocalNormal, mxWorld);
	vsInput.mv4LocalNormal.w = 0.0f;
	stOutput.m_v4Normal = normalize( stOutput.m_v4Normal );
	
	stOutput.mxModel2World = vsInput.mxModel2World;
	stOutput.m_v2UV = vsInput.mv2UV;
	stOutput.mv3Albedo = vsInput.mv4Albedo.xyz;
	stOutput.mfMetallic = vsInput.mfMetallic;
	stOutput.mfRoughness = vsInput.mfRoughness;
	stOutput.mfAO = vsInput.mfAO;
	return stOutput;
}