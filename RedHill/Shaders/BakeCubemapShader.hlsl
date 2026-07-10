#include "Fullscreen.hlsli"
Texture2D g_equirect : register(t0);
SamplerState g_sampler : register(s0);

static const float PI = 3.14159265;

float4 PSMain(PixelInputType input) : SV_TARGET
{
    float3 dir = normalize(input.dir);
    float u = 0.5 - atan2(dir.z,dir.x) * (1.0 / (2.0 * PI));
    float v = 0.5 - asin(dir.y) * (1.0 / PI);
    float2 uv = float2(u, v);
    return g_equirect.SampleLevel(g_sampler, uv,0);
}