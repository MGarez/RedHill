cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 mvp;
    float4x4 model;
    float4x4 invVP;
    float4x4 lightVP;
    float3 cameraPosition;
    float screenWidth;
    float3 lightPosition;
    float screenHeight;
    int castShadows;
    float shadowMapSize;
};