cbuffer CBuffer0
{
    float3 pos;
    float padding;
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
	
	output.position = float4(input.position, 1) + float4(pos, 0);
	output.color = input.color;
	
	return output;
}