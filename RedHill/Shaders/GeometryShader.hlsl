#include "CommonSceneCB.hlsli"

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float3 normal: NORMAL;
    float4 tangent: TANGENT;
};

Texture2D g_albedoTexture : register(t0);
Texture2D g_normalTexture : register(t1);
Texture2D g_metalRoughnessTexture : register(t2);
Texture2D g_aoTexture : register(t3);
SamplerState g_sampler : register(s0);

struct GBufferOutput
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

PixelInputType VSMain(float4 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 tangent : TANGENT)
{
	PixelInputType output;

	output.position = mul(position,mvp);
	output.uv = uv;
    //Here we multiply the normal by the model matrix, this works because for now the model matrix is only a rotation and translation matrix,
    // when we need to support scaling we will need to use the inverse transpose of the model matrix instead.
    output.normal = mul(normal, (float3x3) model);
    output.tangent = float4(mul(tangent.xyz, (float3x3)model), tangent.w);

	return output;
}

GBufferOutput PSMain(PixelInputType input)
{
	GBufferOutput output;

    output.albedo = g_albedoTexture.Sample(g_sampler,input.uv);
    float3 N = normalize(input.normal);
    float3 T = normalize(input.tangent.xyz);
    float3 B = cross(N,T) * input.tangent.w;
    float3 normalTexture = g_normalTexture.Sample(g_sampler, input.uv).xyz * 2.0 - 1.0;
    normalTexture.y = -normalTexture.y;
    float3 convertedNormal = normalize(mul(normalTexture, float3x3(T, B, N)));
    output.normal = float4(convertedNormal, 0.0);

    float4 metrough = g_metalRoughnessTexture.Sample(g_sampler,input.uv);
    float ao =  g_aoTexture.Sample(g_sampler,input.uv).x;

    output.material = float4(metrough.b, metrough.g, ao, 1.0);

    return output;
}