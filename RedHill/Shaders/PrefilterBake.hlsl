#include "Fullscreen.hlsli"

TextureCube g_cubemap : register(t0);
SamplerState g_sampler : register(s0);

cbuffer RoughnessCB : register(b1)
{
    float g_roughness;
    float3 pad;
};

static const float PI = 3.14159265359;
static const uint SAMPLE_COUNT = 1024u;

float2 Hammersley(uint i, uint n)
{
    float vdc = float(reversebits(i)) * 2.3283064365e-10; // /2^32
    return float2(float(i) / float(n), vdc);
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosT = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinT = sqrt(1.0 - cosT * cosT);

    float3 H = float3(cos(phi) * sinT, sin(phi) * sinT, cosT); // tangent space, +Z = N

    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitan = cross(N, tangent);
    return normalize(tangent * H.x + bitan * H.y + N * H.z);
}

float NormalDistribution(float3 n, float3 h, float roughness)
{
    float alpha = roughness * roughness;
    float squaredAlpha = alpha * alpha;

    float NdotH = max(dot(n, h), 0.0);
    float squaredNdotH = NdotH * NdotH;
    float denom = (squaredNdotH * (squaredAlpha - 1.0)) + 1.0;
    denom = PI * denom * denom;
    return squaredAlpha / denom;
}

float4 PSMain(PixelInputType input) : SV_TARGET
{
    float3 N = normalize(input.dir);
    float3 V = N;
    float3 R = N;

    uint w, h, levels;
    g_cubemap.GetDimensions(0, w, h, levels);
    float saTexel = 4.0 * PI / (6.0 * w * w);

    float3 color = float3(0.0, 0.0, 0.0);
    float weight = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, g_roughness);
        float3 L = reflect(-V, H);

        float NdotL = dot(N, L);
        if (NdotL > 0.0)
        {
            float D = NormalDistribution(N, H, g_roughness);

            float NdotH = saturate(dot(N, H));
            float VdotH = saturate(dot(V, H));

            float pdf = (D * NdotH) / (4.0 * VdotH) + 1e-4;

            float saS = 1.0 / (float(SAMPLE_COUNT) * pdf + 1e-4);
            float mip = g_roughness <= 0.0 ? 0.0 : 0.5 * log2(saS / saTexel);

            color += g_cubemap.SampleLevel(g_sampler, L, mip).rgb * NdotL;
            weight += NdotL;
        }
    }
    // Cover the degenerate case where the weight is zero to avoid division by zero
    weight = max(weight, 0.001);
    return float4(color / weight, 1.0);
}