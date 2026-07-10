cbuffer FaceCB : register(b0)
{
    float3 right;
    float pad0;
    float3 up;
    float pad1;
    float3 fwd;
    float pad2;
};

struct PixelInputType
{
    float4 pos : SV_POSITION;
    float3 dir : TEXCOORD;
};


PixelInputType VSMain(uint id : SV_VertexID)
{
    PixelInputType output;

    float2 clipXY;
    if (id == 0)
    {
        clipXY = float2(-1.0, -1.0);
    }
    else if (id == 1)
    {
        clipXY = float2(-1.0, 3.0);
    }
    else
    {
        clipXY = float2(3.0, -1.0);
    }
    output.pos = float4(clipXY, 0.0, 1.0);
    output.dir = right * clipXY.x + up * clipXY.y + fwd * 1.0;
    return output;
}
