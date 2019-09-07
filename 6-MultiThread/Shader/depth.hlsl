
cbuffer MatrixBuffer : register(b0)
{
	matrix mxWVP;
};

struct VertexInputType
{
    float4 position : POSITION;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float4 depthPosition : TEXTURE0;
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType DepthVertexShader(VertexInputType input)
{
    PixelInputType output;

	// Change the position vector to be 4 units for proper matrix calculations.
    input.position.w = 1.0f;

	// Calculate the position of the vertex against the world, view, and projection matrices.
    output.position = mul(input.position, mxWVP);
 
	// Store the position value in a second input value for depth value calculations.
	output.depthPosition = output.position;
	
	return output;
}

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 DepthPixelShader(PixelInputType input) : SV_TARGET
{
	float depthValue;
	float4 color;


	// Get the depth value of the pixel by dividing the Z pixel depth by the homogeneous W coordinate.
	depthValue = input.depthPosition.z / input.depthPosition.w;

	color = float4(depthValue, depthValue, depthValue, 1.0f);

	return color;
}
