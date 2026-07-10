#include "CommonSceneCB.hlsli"

TextureCube g_cube : register(t0);
SamplerState g_sampler : register(s0);

struct VSIn
{
    uint vertexId : SV_VertexID;
};

struct PSIn
{
    float4 position : SV_POSITION;
};

float3 ACESFilm(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

PSIn VSMain(VSIn input)
{
    PSIn output;
    if (input.vertexId == 0)
    {
        output.position = float4(-1.0, -1.0, 0.0, 1.0);
    }
    else if (input.vertexId == 1)
    {
        output.position = float4(-1.0, 3.0, 0.0, 1.0);
    }
    else if (input.vertexId == 2)
    {
        output.position = float4(3.0, -1.0, 0.0, 1.0);
    }
    return output;
}

float4 PSMain(PSIn input) : SV_Target
{

    float2 screenUV = input.position.xy / float2(screenWidth, screenHeight);

    float2 ndc;
    ndc.x = screenUV.x * 2.0 - 1.0;
    ndc.y = 1 - screenUV.y * 2.0;

    float4 farPoint = mul(float4(ndc.x, ndc.y, 0.0, 1.0), invVP);
    farPoint.xyz /= farPoint.w;

    float3 dir = normalize(farPoint.xyz - cameraPosition);
    float3 color = g_cube.SampleLevel(g_sampler, dir,0).rgb;

    color = ACESFilm(color);
    float aux = 1.0 / 2.2;
    color = pow(color, float3(aux, aux, aux));

    return float4(color, 1.0);
}