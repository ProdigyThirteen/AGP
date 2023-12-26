Texture2D texture0 : register(t0);
TextureCube skybox0 : register(t1);
sampler sampler0;

float4 main(float4 position : SV_Position, 
            float4 colour   : COLOR, 
            float2 uv       : TEXCOORD0,
            float3 reflectedUVW : TEXCOORD1)
            : SV_TARGET
{
    float4 sampled = texture0.Sample(sampler0, uv);
    float4 reflectedSample = skybox0.Sample(sampler0, reflectedUVW);
    float4 combined = ((colour * sampled) * 0.9) + (reflectedSample * 0.1);
    
    return saturate(combined);

}