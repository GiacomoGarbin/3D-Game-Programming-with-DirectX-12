#define LIGHT_MAX_COUNT 16

#ifndef LIGHT_DIR_COUNT
    #define LIGHT_DIR_COUNT 1
#endif

#ifndef LIGHT_POINT_COUNT
    #define LIGHT_POINT_COUNT 0
#endif

#ifndef LIGHT_SPOT_COUNT
    #define LIGHT_SPOT_COUNT 0
#endif

struct Light
{
    float3 strength;
    float FalloffStart;
    float3 direction;
    float FalloffEnd;
    float3 position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float shininess;
};

float LightAttenuation(float x, float a, float b)
{
    return saturate((b - x) / (b - a));
}

float3 SchlickFresnel(float3 R0, float3 normal, float3 light)
{
    float angle = saturate(dot(normal, light));

    float f0 = 1.0f - angle;
    float3 r = R0 + (1.0f - R0) * (f0*f0*f0*f0*f0);

    return r;
}

float3 BlinnPhong(float3 LightStrength, float3 LightVec, float3 normal, float3 ToEye, Material material)
{
    const float m = material.shininess * 256.0f;
    float3 HalfVec = normalize(ToEye + LightVec);

    float roughness = (m + 8.0f) * pow(max(dot(HalfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnel = SchlickFresnel(material.FresnelR0, HalfVec, LightVec);

    float3 albedo = fresnel * roughness;
    albedo = albedo / (albedo + 1.0f);

    return (material.DiffuseAlbedo.rgb + albedo) * LightStrength;
}

float3 ComputeDirectionalLight(Light light, Material material, float3 normal, float3 ToEye)
{
    // the light vector aims opposite the direction the light rays travel
    float3 LightVec = -light.direction;

    // scale light down by Lambert's cosine law.
    float ndotl = max(dot(LightVec, normal), 0.0f);
    float3 LightStrength = light.strength * ndotl;

    return BlinnPhong(LightStrength, LightVec, normal, ToEye, material);
}

float4 ComputeLighting(Light lights[LIGHT_MAX_COUNT],
                       Material material,
                       float3 position,
                       float3 normal,
                       float3 ToEye,
                       float3 ShadowFactor)
{
    float3 result = 0.0f;

    int i = 0;

#if (LIGHT_DIR_COUNT > 0)
    for(i = 0; i < LIGHT_DIR_COUNT; ++i)
    {
        result += ShadowFactor[i] * ComputeDirectionalLight(lights[i], material, normal, ToEye);
    }
#endif

// #if (LIGHT_POINT_COUNT > 0)
//     for(i = LIGHT_DIR_COUNT; i < LIGHT_DIR_COUNT + LIGHT_POINT_COUNT; ++i)
//     {
//         result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
//     }
// #endif

// #if (LIGHT_POINT_COUNT > 0)
//     for(i = LIGHT_DIR_COUNT + LIGHT_POINT_COUNT; i < LIGHT_DIR_COUNT + LIGHT_POINT_COUNT + LIGHT_SPOT_COUNT; ++i)
//     {
//         result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
//     }
// #endif

    return float4(result, 0.0f);
}