cbuffer CBuffer0
{
    matrix WVP;
}

struct VIn
{
	float3 position : POSITION;
	float4 color : COLOR;
};

struct VOut
{
	float4 position : SV_Position;
	float4 color : COLOR;
};

VOut main(VIn input)
{
	VOut output;
	
    output.position = mul(WVP, float4(input.position, 1));
	output.color = input.color;
	
	return output;
}