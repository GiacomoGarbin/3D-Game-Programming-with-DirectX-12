#include "LightingUtils.hlsl"

cbuffer ObjectCB : register(b0)
{
    float4x4 gWorld;
};

cbuffer MaterialCB : register(b1)
{
	float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float  gRoughness;
	// float4x4 gMatTransform;
};

cbuffer MainPassCB : register(b2)
{
	float4x4 gView;
	float4x4 gViewInverse;
	float4x4 gProj;
	float4x4 gProjInverse;
	float4x4 gViewProj;
	float4x4 gViewProjInverse;
	float3 gEyePositionW;
	float padding;
	float2 gRenderTargetSize;
	float2 gRenderTargetSizeInverse;
	float gNearPlane;
	float gFarPlane;
	float gDeltaTime;
	float gTotalTime;

	float4 gAmbientLight;
	Light gLights[LIGHT_MAX_COUNT];
};

struct VertexIn
{
    float3 PositionL : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float4 PositionH : SV_POSITION;
	float3 PositionW : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

	float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);
	vout.PositionW = PositionW.xyz;
    vout.PositionH = mul(PositionW, gViewProj);

    vout.NormalW = mul(vin.NormalL, (float3x3)(gWorld));

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	pin.NormalW = normalize(pin.NormalW);

	float3 ToEyeW = normalize(gEyePositionW - pin.PositionW);

	// indirect lighting
	float4 ambient = gAmbientLight * gDiffuseAlbedo;

	// direct lighting
	const float shininess = 1.0f - gRoughness;
	Material material = { gDiffuseAlbedo, gFresnelR0, shininess };
	float3 ShadowFactor = 1.0f;
	float4 direct = ComputeLighting(gLights, material, pin.PositionW, pin.NormalW, ToEyeW, ShadowFactor);
	
	float4 result = ambient + direct;
	result.a = gDiffuseAlbedo.a;

    return result;
}