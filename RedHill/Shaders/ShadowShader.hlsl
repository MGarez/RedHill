#include "CommonSceneCB.hlsli"

struct PixelInputType
{
    float4 position : SV_POSITION;
};

PixelInputType VSMain(float4 position : POSITION)
{
    PixelInputType output;
    output.position = mul(position, lightVP);
    return output;
}