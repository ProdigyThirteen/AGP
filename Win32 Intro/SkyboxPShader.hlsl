TextureCube cube0;
sampler sampler0;

float4 main(float4 position : SV_Position, float4 colour : COLOR, float3 uvw : TEXCOORD) : SV_TARGET
{
	float4 sampled = cube0.Sample(sampler0, uvw);
	return colour * sampled;
}