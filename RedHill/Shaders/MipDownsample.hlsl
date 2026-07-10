Texture2DArray<float4> g_src : register(t0);
RWTexture2DArray<float4> g_dst : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint w;
    uint h;
    uint faces;
    g_dst.GetDimensions(w, h, faces);
    if (id.x >= w || id.y >= h)
    {
        return;
    }

    int2 sourcePos = int2(id.x * 2, id.y * 2);

    // Box filter downsample
    float4 c = g_src.Load(int4(sourcePos, id.z, 0)) +
              g_src.Load(int4(sourcePos.x + 1, sourcePos.y, id.z, 0)) +
              g_src.Load(int4(sourcePos.x, sourcePos.y + 1, id.z, 0)) +
              g_src.Load(int4(sourcePos.x + 1, sourcePos.y + 1, id.z, 0));
    c *= 0.25;

    g_dst[uint3(id.xy, id.z)] = c;
}