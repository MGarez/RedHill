#include "CommonSceneCB.hlsli"

Texture2D rt_albedo : register(t0);
Texture2D rt_normal : register(t1);
Texture2D rt_material : register(t2);
Texture2D rt_depth : register(t3);
Texture2D g_brdfLUT : register(t4);
Texture2D g_shadowMap : register(t5);
TextureCube g_irradianceMap : register(t6);
TextureCube g_prefilterMap : register(t7);

SamplerState g_sampler : register(s0);
SamplerComparisonState g_shadowSampler : register(s1);

struct VSIn
{
    uint vertexId : SV_VertexID;
};

struct PSIn
{
    float4 position : SV_POSITION;
};

static const float PI = 3.14159265;
static const uint PREFILTER_MIPS = 5;
static const float SHADOW_BIAS_MIN = 0.002; // floor bias, prevents peter-panning
static const float SHADOW_BIAS_MAX = 0.010; // grazing-angle bias, kills acne

// This function expects alpha=roughness^2
float NormalDistribution(float3 n, float3 h, float alpha)
{
    float squaredAlpha = alpha * alpha;

    float NdotH = max(dot(n,h), 0.0);
    float squaredNdotH = NdotH * NdotH;
    float denom = (squaredNdotH * (squaredAlpha - 1.0)) + 1.0;
    denom = PI * denom * denom;
    return squaredAlpha / denom;
}

float SchlickGeometry(float3 w, float3 n, float k)
{
    float WdotN = max(dot(w, n), 0.0);
    float denom = (WdotN * (1 -k)) + k;
    return WdotN / denom;
}
float SmithGeometry(float3 l, float3 v, float3 n, float k)
{
    float gl = SchlickGeometry(l,n,k);
    float gv = SchlickGeometry(v,n,k);

    return gl * gv;
}

float3 SchlickFresnel(float3 v, float3 h, float3 f0)
{
    float VdotH = max(dot(v,h), 0.0);
    float power = pow(1 - VdotH, 5.0);
    return f0 + ((1-f0) * power);
}

float3 SchlickFresnelRoughness(float cosT, float3 f0, float rough)
{
    float3 ceil = max(float3(1.0 - rough, 1.0 - rough, 1.0 - rough), f0);
    return f0 + (ceil - f0) * pow(1.0 - cosT, 5.0);
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float ComputeShadow(float3 surfacePosition, float cosTheta)
{
    float4 lightClip = mul(float4(surfacePosition, 1.0), lightVP);
    lightClip.xyz /= lightClip.w; // perspective divide (always 1 for now but just in case for the future)

    float2 uv = float2(lightClip.x * 0.5 + 0.5, -lightClip.y * 0.5 + 0.5);

    if (any(uv < 0.0) || any(uv > 1.0) || lightClip.z < 0.0 || lightClip.z > 1.0)
        return 1.0;

    float bias = max(SHADOW_BIAS_MIN, SHADOW_BIAS_MAX * (1.0 - cosTheta));
    float referenceDepth = lightClip.z + bias;

    float2 texel = 1.0 / shadowMapSize;

    float sum = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            sum += g_shadowMap.SampleCmpLevelZero(g_shadowSampler, uv + float2(x, y) * texel, referenceDepth);
        }
    }

    return sum/9.0;
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

float4 PSMain(PSIn input): SV_TARGET
{
    // Reconstruct surface position with the depth buffer
    float depth = rt_depth.Load(int3(input.position.xy,0)).r;

    float2 screenUV = input.position.xy / float2(screenWidth, screenHeight);

    float2 ndc;
    ndc.x = screenUV.x * 2.0 - 1.0;
    ndc.y = 1 - screenUV.y *2.0;

    float4 initialPosition = float4(ndc.x, ndc.y, depth, 1.0);
    float4 surfacePosition = mul(initialPosition, invVP);
    surfacePosition.xyz /= surfacePosition.w;

    // Load RT info
    float3 albedo = rt_albedo.Load(int3(input.position.xy, 0)).rgb;
    float3 normal = rt_normal.Load(int3(input.position.xy, 0)).rgb;
    float3 material = rt_material.Load(int3(input.position.xy, 0)).rgb;
    float metallic = material.r;
    float roughness = material.g;
    float ao = material.b;

    // Hard-coded light data
    float3 lightColor = float3(12.47, 16.31, 20.79);

    float3 lVector = normalize(lightPosition - surfacePosition.xyz);
    float3 vVector = normalize(cameraPosition - surfacePosition.xyz);
    float3 hVector = normalize(vVector + lVector);

    float cosTheta = max(dot(normal, lVector), 0.0);
    float attenuation =  1.0; // no attenuation yet
    float3 radiance = lightColor * cosTheta * attenuation;

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    // IBL
    float3 irradiance = g_irradianceMap.SampleLevel(g_sampler, normal, 0).rgb;

    // Cook-Torrance specular
    float NDF = NormalDistribution(normal, hVector, roughness * roughness);
    // For direct light the k factor is:
    float k = (roughness + 1.0) * (roughness + 1.0) / 8;
    float G = SmithGeometry(lVector, vVector, normal, k);
    float3 F = SchlickFresnel(vVector, hVector, F0);

    float3 kd =  (float3(1.0,1.0,1.0) - F) * (1.0 - metallic);

    float3 numerator = NDF * G * F;
    float denominator = 4 * max(dot(normal, lVector), 0.0) * max(dot(normal, vVector), 0.0) + 0.0000001;
    float3 specular = numerator / denominator;

    float3 diffuse = albedo / PI;

    float3 brdf = kd * diffuse + specular;

    float shadow = 1.0;

     // Compute shadow factor (if not in the sphere test grid mode)
    if (castShadows != 0)
    {
        shadow = ComputeShadow(surfacePosition.xyz, cosTheta);
    }

    float3 directLight = shadow *brdf * radiance;

    // view-normal angle and reflection vector for the environment lookup
    float NdotV = max(dot(normal, vVector), 0.0);
    float3 rVector = reflect(-vVector, normal);

    // pick the prefilter mip
    float lod = roughness * float(PREFILTER_MIPS - 1u);
    float3 prefilter = g_prefilterMap.SampleLevel(g_sampler, rVector, lod).rgb;

    // load the lut value
    float2 brdfLUT = g_brdfLUT.Sample(g_sampler, float2(NdotV, roughness)).rg;

    float3 specularIBL = prefilter * (F0 * brdfLUT.x + brdfLUT.y);

    float3 FIBL = SchlickFresnelRoughness(NdotV, F0, roughness);
    float3 kdIBL = (float3(1.0, 1.0, 1.0) - FIBL) * (1.0 - metallic);

    float3 diffuseIBL = kdIBL * irradiance * albedo;
    float3 ambientLight = (diffuseIBL + specularIBL) * ao;

    float3 finalLight = directLight + ambientLight;

    float3 correctedColor = ACESFilm(finalLight);
    float aux = 1.0/2.2;
    correctedColor = pow(correctedColor, float3(aux, aux, aux));

    return float4(correctedColor, 1.0);
}