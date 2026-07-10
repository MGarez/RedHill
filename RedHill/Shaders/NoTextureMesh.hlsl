#include "CommonSceneCB.hlsli"

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

static float3 g_flatColor = float3(0.6, 0.6, 0.6);

struct GBufferOutput
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

PixelInputType VSMain(float4 position : POSITION, float3 normal : NORMAL)
{
    PixelInputType output;
    output.position = mul(position, mvp);
    output.normal = normal;
    return output;
}

GBufferOutput PSMain(PixelInputType input)
{
    GBufferOutput output;

    output.albedo = float4(g_flatColor, 1.0);

    output.normal = float4(normalize(input.normal), 0.0);

    output.material = float4(0.0, 1.0, 1.0, 1.0); // 0 metallic and 1 roughness

    return output;
}
