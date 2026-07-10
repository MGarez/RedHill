#include "Fullscreen.hlsli"

TextureCube g_cubemap : register(t0);
SamplerState g_sampler : register(s0);

static const float PI = 3.14159265;

float4 PSMain(PixelInputType input) : SV_TARGET
{
    float3 N = normalize(input.dir);
    float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);

    float3 irradiance = float3(0.0, 0.0, 0.0);
    uint sampleCount = 0;
    const float sampleDelta = 0.025;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        float sinPhi;
        float cosPhi;
        sincos(phi, sinPhi, cosPhi);

        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float sinTheta;
            float cosTheta;
            sincos(theta, sinTheta, cosTheta);

            float3 tangentSample = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            float3 radiance = g_cubemap.SampleLevel(g_sampler, sampleVec, 6.0).rgb; // sample a low mip to suppress bright-spot (firefly) artifacts during convolution
            irradiance += radiance * cosTheta * sinTheta;
            ++sampleCount;
        }
    }

    irradiance = PI * irradiance / (float) sampleCount;
    return float4(irradiance, 1.0);

}