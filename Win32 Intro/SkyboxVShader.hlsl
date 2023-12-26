struct VIn
{
	float4 position : POSITION;
	float4 colour   : COLOR;
	float2 uv       : TEXCOORD;
	float3 normal   : NORMAL;
};

struct VOut
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 uvw      : TEXCOORD;
};

cbuffer CBuffer0
{
    matrix WVP;
};

VOut main(VIn input)
{
    VOut output;

    output.position = mul(WVP, input.position);
    output.color    = input.colour;
    output.uvw      = input.position.xyz;

    return output;
}