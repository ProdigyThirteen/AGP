float4 main( float4 pos : SV_Position ) : SV_TARGET
{
    return float4(pos.x / 800, pos.y / 600, 0.0f, 1.0f);
}