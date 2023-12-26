#define MAX_POINT_LIGHTS 8

struct PointLight
{
    float4 position;
    float4 color;
    float strength;
	
	bool enabled;
	
	float2 padding;
};

cbuffer CBuffer0
{
    matrix WVP;
	float4 ambientLightColor;
    float4 directionalLightDir;
	float4 directionalLightColor;
	
	PointLight pointLights[MAX_POINT_LIGHTS];
}

struct VIn
{
	float3 position : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

struct VOut
{
	float4 position : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

VOut main(VIn input)
{
    VOut output;
	
    output.position = mul(WVP, float4(input.position, 1));
    output.uv = input.uv;
	
	// Lighting
    float diffuseAmount = dot(directionalLightDir.xyz, input.normal);
    diffuseAmount = saturate(diffuseAmount);
	
    float3 directionalFinal = directionalLightColor * diffuseAmount;
	
	// point light
    float3 pointFinal = float3(0, 0, 0);
    
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
		if (!pointLights[i].enabled)
			continue;
		
        float4 pointLightDir = normalize(pointLights[i].position - float4(input.position, 1));
        float pointLightDistance = length(pointLights[i].position - float4(input.position, 1));
        float pointLightAttenuation = pointLights[i].strength / (pointLightDistance * pointLightDistance + pointLights[i].strength);
		float pointAmount = dot(pointLightDir.xyz, input.normal) * pointLightAttenuation;
		pointAmount = saturate(pointAmount);
		pointFinal += pointLights[i].color * pointAmount * 5;
	}
	
	
    output.color = saturate(ambientLightColor + float4(directionalFinal, 1) + float4(pointFinal, 1));
	
	return output;
}