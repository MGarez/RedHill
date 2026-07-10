static const float PI = 3.14159265359;
static const uint SAMPLE_COUNT = 1024u;

struct PixelInputType
{
    float4 pos : SV_Position;
};

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

float SchlickGeometry(float dot, float k)
{
    float denom = (dot * (1 - k)) + k;
    return dot / denom;
}

float SmithGeometry(float NdotL, float NdotV, float k)
{
    float gl = SchlickGeometry(NdotL, k);
    float gv = SchlickGeometry(NdotV, k);
    return gl * gv;
}

float2 Integrate(float NdotV, float rough)
{
    float3 v = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    float3 n = float3(0.0, 0.0, 1.0);

    float scale = 0.0;
    float bias = 0.0;
    float k = (rough * rough) / 2.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 h = ImportanceSampleGGX(Xi, n, rough);
        float3 l = reflect(-v, h);

        float NdotL = max(l.z, 0.0);
        if (NdotL > 0.0)
        {
            float NdotH = max(h.z, 0.0);
            float VdotH = max(dot(v, h), 0.0);
            float G = SmithGeometry(NdotL, NdotV, k);
            float denom = max(NdotH * NdotV, 1e-4);
            float Gv = (G * VdotH) / denom;
            float Fc = pow(1.0 - VdotH, 5.0);
            scale += (1.0 - Fc) * Gv;
            bias += Fc * Gv;
        }
    }
    return float2(scale, bias) / float(SAMPLE_COUNT);
}

PixelInputType VSMain(uint id : SV_VertexID)
{
    PixelInputType output;
    if (id == 0)
    {
        output.pos = float4(-1.0, -1.0, 0.0, 1.0);
    }
    else if (id == 1)
    {
        output.pos = float4(-1.0, 3.0, 0.0, 1.0);
    }
    else if (id == 2)
    {
        output.pos = float4(3.0, -1.0, 0.0, 1.0);
    }
    return output;
}

float2 PSMain(PixelInputType input) : SV_Target
{
    float2 uv = input.pos.xy / 512.0;
    float clamped = max(uv.x, 0.0001);
    return Integrate(clamped, uv.y);
}