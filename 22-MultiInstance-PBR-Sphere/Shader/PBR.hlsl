// Simplified PBR HLSL 

#define GRS_LIGHT_COUNT 4

static const float PI = 3.14159265359;

cbuffer MVPBuffer : register( b0 )
{
    float4x4 mxWorld;                   //世界矩阵，这里其实是Model->World的转换矩阵
    float4x4 mxView;                    //视矩阵
    float4x4 mxProjection;				//投影矩阵
    float4x4 mxViewProj;                //视矩阵*投影
    float4x4 mxMVP;                     //世界*视矩阵*投影
};

// Camera
cbuffer ST_CB_CAMERA: register( b1 )
{
    float4 m_v4CameraPos;
};

// lights
cbuffer ST_CB_LIGHTS : register( b2 )
{
    float4 m_v4LightPos[GRS_LIGHT_COUNT];
    float4 m_v4LightColors[GRS_LIGHT_COUNT];
};


struct PSInput
{
    float4	    m_v4PosWVP	    : SV_POSITION;
    float4	    m_v4PosWorld    : POSITION;
    float4	    m_v4Normal	    : NORMAL;
    float2	    m_v2UV		    : TEXCOORD;
    float4x4	mxModel2World	: WORLD;
    float3	    mv3Albedo	    : COLOR0;    // 反射率
    float	    mfMetallic      : COLOR1;    // 金属度
    float	    mfRoughness     : COLOR2;    // 粗糙度
    float	    mfAO            : COLOR3;
};

float3 fresnelSchlick( float cosTheta, float3 F0 )
{
	return F0 + ( 1.0 - F0 ) * pow( 1.0 - cosTheta, 5.0 );
}

float DistributionGGX( float3 N, float3 H, float fRoughness )
{
    float a = fRoughness * fRoughness;
    float a2 = a * a;
    float NdotH = max( dot( N, H ), 0.0 );
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = ( NdotH2 * ( a2 - 1.0 ) + 1.0 );
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX( float NdotV, float fRoughness )
{
    float r = ( fRoughness + 1.0 );
    float k = ( r * r ) / 8.0;

    float num = NdotV;
    float denom = NdotV * ( 1.0 - k ) + k;

    return num / denom;
}

float GeometrySmith( float3 N, float3 V, float3 L, float fRoughness )
{
    float NdotV = max( dot( N, V ), 0.0 );
    float NdotL = max( dot( N, L ), 0.0 );
    float ggx2 = GeometrySchlickGGX( NdotV, fRoughness );
    float ggx1 = GeometrySchlickGGX( NdotL, fRoughness );

    return ggx1 * ggx2;
}

float4 PSMain( PSInput stPSInput ) : SV_TARGET
{
    // 插值生成的光滑法线
    float3 N = normalize( stPSInput.m_v4Normal.xyz );
    float3 V = normalize( m_v4CameraPos.xyz - stPSInput.m_v4PosWorld.xyz );
    float3 F0 = float3( 0.04f, 0.04f, 0.04f );
    float3 v3Albedo = pow( stPSInput.mv3Albedo, 2.2f ); // 将漫反射转换到线性空间
   
    F0 = lerp( F0, v3Albedo, stPSInput.mfMetallic );

    // reflectance equation
    float3 Lo = float3( 0.0f, 0.0f, 0.0f );

    for ( int i = 0; i < GRS_LIGHT_COUNT; ++i )
    {// 对每一个光源求解光照积分方程
        // 点光源的情况
        // 因为是多实例渲染，所以对每个光源做下位置变换，这样相当于每个实例有了自己的光源
        // 注意这不是标准操作，一定要注意正式的代码中不要这样做
        float4 v4LightWorldPos = mul( m_v4LightPos[i], stPSInput.mxModel2World );
        v4LightWorldPos = mul( v4LightWorldPos, mxWorld );

        float3 LightDir = v4LightWorldPos.xyz - stPSInput.m_v4PosWorld.xyz;
        float3 L = normalize( LightDir );
        float3 H = normalize( V + L );
        float distance = length( LightDir );
        float attenuation = 1.0 / ( distance * distance );  //距离平方衰减
        float3 radiance = m_v4LightColors[i].xyz * attenuation;

        // cook-torrance brdf
        float NDF = DistributionGGX( N, H, stPSInput.mfRoughness );
        float G = GeometrySmith( N, V, L, stPSInput.mfRoughness );
        float3 F = fresnelSchlick( max( dot( H, V ), 0.0 ), F0 );

        float3 kS = F;
        float3 kD = float3( 1.0f,1.0f,1.0f ) - kS;
        kD *= 1.0 - stPSInput.mfMetallic;

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max( dot( N, V ), 0.0 ) * max( dot( N, L ), 0.0 );
        float3 specular = numerator / max( denominator, 0.001 );

        // add to outgoing radiance Lo
        float NdotL = max( dot( N, L ), 0.0 );
        Lo += ( kD * v3Albedo / PI + specular ) * radiance * NdotL;
    }

    float3 ambient = float3( 0.03f, 0.03f, 0.03f ) * v3Albedo * stPSInput.mfAO;
    float3 color = ambient + Lo;

    color = color / ( color + float3( 1.0f,1.0f,1.0f ) );
    color = pow( color, float3( 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f ) );

    return float4( color, 1.0 );
}

