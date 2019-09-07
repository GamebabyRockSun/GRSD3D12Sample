

/////////////
// GLOBALS //
/////////////
cbuffer MatrixBuffer : register(b0)
{
	matrix mxWorld;
	matrix mxWVP;

	//matrix mxView;
	//matrix mxProjection;

	matrix mxLightVP;
	matrix mxLightVP2;

	//matrix mxLightView;
	//matrix mxLightProjection;
	//matrix mxLightView2;
	//matrix mxLightProjection2;
};


//////////////////////
// CONSTANT BUFFERS //
//////////////////////
cbuffer LightBuffer2 : register(b1)
{
	float3	v3LightPosition;
	float	fPadding1;
	float3	v3LightPosition2;
	float	fPadding2;
};

//////////////////////
// CONSTANT BUFFERS //
//////////////////////
cbuffer LightBuffer : register(b2)
{
	float4 c4Ambient;
	float4 c4Diffuse;
	float4 c4Diffuse2;
};

//////////////
// TEXTURES //
//////////////
Texture2D t2dShader		: register(t0);
Texture2D t2dDepthMap	: register(t1);
Texture2D t2dDepthMap2	: register(t2);

///////////////////
// SAMPLE STATES //
///////////////////
SamplerState SampleClamp : register(s0);
SamplerState SampleWrap  : register(s1);

//////////////
// TYPEDEFS //
//////////////
struct VertexInputType
{
    float4 v4Position	: POSITION;
    float2 v2TexCoord	: TEXCOORD0;
	float3 v3Normal		: NORMAL;
};

struct PixelInputType
{
    float4 v4Position			: SV_POSITION;
    float2 v2TexCoord			: TEXCOORD0;
	float3 v3Normal				: NORMAL;
    float4 v4LightViewPosition	: TEXCOORD1;
	float3 v3LightPos			: TEXCOORD2;
    float4 v4LightViewPosition2	: TEXCOORD3;
	float3 v3LightPos2			: TEXCOORD4;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType ShadowVertexShader(VertexInputType stInput)
{
    PixelInputType stOutput;
	float4 v4WorldPosition;
    
	// Change the v4Position vector to be 4 units for proper matrix calculations.
    stInput.v4Position.w = 1.0f;
	// Calculate the v4Position of the vertex against the world, view, and projection matrices.
    stOutput.v4Position = mul(stInput.v4Position, mxWVP);
      
	// Calculate the v4Position of the vertice as viewed by the light source.
    stOutput.v4LightViewPosition = mul(stInput.v4Position, mxWorld);
    stOutput.v4LightViewPosition = mul(stOutput.v4LightViewPosition, mxLightVP);
    //stOutput.v4LightViewPosition = mul(stOutput.v4LightViewPosition, mxLightProjection);

	// Calculate the v4Position of the vertice as viewed by the second light source.
    stOutput.v4LightViewPosition2 = mul(stInput.v4Position, mxWorld);
    stOutput.v4LightViewPosition2 = mul(stOutput.v4LightViewPosition2, mxLightVP2);
    //stOutput.v4LightViewPosition2 = mul(stOutput.v4LightViewPosition2, mxLightProjection2);

	// Store the texture coordinates for the pixel shader.
    stOutput.v2TexCoord = stInput.v2TexCoord;
    
	// Calculate the v3Normal vector against the world matrix only.
    stOutput.v3Normal = mul(stInput.v3Normal, (float3x3)mxWorld);
	
    // Normalize the v3Normal vector.
    stOutput.v3Normal = normalize(stOutput.v3Normal);

    // Calculate the v4Position of the vertex in the world.
    v4WorldPosition = mul(stInput.v4Position, mxWorld);

    // Determine the light v4Position based on the v4Position of the light and the v4Position of the vertex in the world.
    stOutput.v3LightPos = v3LightPosition.xyz - v4WorldPosition.xyz;

    // Normalize the light v4Position vector.
    stOutput.v3LightPos = normalize(stOutput.v3LightPos);

    // Determine the second light v4Position based on the v4Position of the light and the v4Position of the vertex in the world.
    stOutput.v3LightPos2 = v3LightPosition2.xyz - v4WorldPosition.xyz;

    // Normalize the second light v4Position vector.
    stOutput.v3LightPos2 = normalize(stOutput.v3LightPos2);

	return stOutput;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 ShadowPixelShader(PixelInputType stInput) : SV_TARGET
{
	float bias;
	float4 color;
	float2 projectTexCoord;
	float depthValue;
	float lightDepthValue;
	float lightIntensity;
	float4 textureColor;


	// Set the bias value for fixing the floating point precision issues.
	bias = 0.00f;

	// Set the default stOutput color to the ambient light value for all pixels.
	color = c4Ambient;

	// Calculate the projected texture coordinates.
	projectTexCoord.x = stInput.v4LightViewPosition.x / stInput.v4LightViewPosition.w / 2.0f + 0.5f;
	projectTexCoord.y = -stInput.v4LightViewPosition.y / stInput.v4LightViewPosition.w / 2.0f + 0.5f;

	// Determine if the projected coordinates are in the 0 to 1 range.  If so then this pixel is in the view of the light.
	if ((saturate(projectTexCoord.x) == projectTexCoord.x) && (saturate(projectTexCoord.y) == projectTexCoord.y))
	{
		// Sample the shadow map depth value from the depth texture using the sampler at the projected texture coordinate location.
		depthValue = t2dDepthMap.Sample(SampleClamp, projectTexCoord).r;

		// Calculate the depth of the light.
		lightDepthValue = stInput.v4LightViewPosition.z / stInput.v4LightViewPosition.w;

		// Subtract the bias from the lightDepthValue.
		lightDepthValue = lightDepthValue - bias;

		// Compare the depth of the shadow map value and the depth of the light to determine whether to shadow or to light this pixel.
		// If the light is in front of the object then light the pixel, if not then shadow this pixel since an object (occluder) is casting a shadow on it.
		if ( lightDepthValue < depthValue )
		{
			// Calculate the amount of light on this pixel.
			lightIntensity = saturate(dot(stInput.v3Normal, stInput.v3LightPos));
			if (lightIntensity > 0.0f)
			{
				// Determine the final diffuse color based on the diffuse color and the amount of light intensity.
				color += (c4Diffuse * lightIntensity);
			}
		}
	}

	// Second light.
	projectTexCoord.x = stInput.v4LightViewPosition2.x / stInput.v4LightViewPosition2.w / 2.0f + 0.5f;
	projectTexCoord.y = -stInput.v4LightViewPosition2.y / stInput.v4LightViewPosition2.w / 2.0f + 0.5f;

	if ((saturate(projectTexCoord.x) == projectTexCoord.x) && (saturate(projectTexCoord.y) == projectTexCoord.y))
	{
		depthValue = t2dDepthMap2.Sample(SampleClamp, projectTexCoord).r;

		lightDepthValue = stInput.v4LightViewPosition2.z / stInput.v4LightViewPosition2.w;
		lightDepthValue = lightDepthValue - bias;

		if (lightDepthValue < depthValue)
		{
			lightIntensity = saturate(dot(stInput.v3Normal, stInput.v3LightPos2));
			if (lightIntensity > 0.0f)
			{
				color += (c4Diffuse2 * lightIntensity);
			}
		}
	}

	// Saturate the final light color.
	color = saturate(color);

	// Sample the pixel color from the texture using the sampler at this texture coordinate location.
	textureColor = t2dShader.Sample(SampleWrap, stInput.v2TexCoord);

	// Combine the light and texture color.
	color = color * textureColor;

	return color;
}