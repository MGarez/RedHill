#include "CommonSceneCB.hlsli"

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    nointerpolation float2 metrough : TEXCOORD0;
};

static float3 g_color = float3(1.0, 0.7, 0.25);
static float g_offset[5] = { 5.0, 2.5, 0.0, -2.5, -5.0 };
static float g_metallic[5] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
static float g_roughness[5] = { 1.0, 0.75, 0.5, 0.25, 0.05 };

struct GBufferOutput
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

PixelInputType VSMain(float4 position : POSITION,  float3 normal : NORMAL, uint instanceID : SV_InstanceID)
{
    PixelInputType output;
    uint col = instanceID % 5;
    uint row = instanceID / 5;
    float xOffset = g_offset[col];
    float zOffset = g_offset[row];

    float4 offsetPosition = position + float4(xOffset, 0.0, zOffset, 0.0);
    output.position = mul(offsetPosition, mvp);
    output.normal = normal;
    output.metrough = float2(g_metallic[col], g_roughness[row]);
    return output;
}

GBufferOutput PSMain(PixelInputType input)
{
    GBufferOutput output;

    output.albedo = float4(g_color, 1.0);

    output.normal = float4(normalize(input.normal), 0.0);

    output.material = float4(input.metrough.x, input.metrough.y, 1.0, 1.0);

    return output;
}
