#include "LightingUtils.hlsl"

Texture2D gDiffuseTexture : register(t0);
SamplerState gSamplerLinearWrap : register(s2);

cbuffer ObjectCB : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexCoordTransform;
};

cbuffer MaterialCB : register(b1)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float  gRoughness;
	float4x4 gMaterialTransform;
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
	float padding1;
	float2 gRenderTargetSize;
	float2 gRenderTargetSizeInverse;
	float gNearPlane;
	float gFarPlane;
	float gDeltaTime;
	float gTotalTime;

	float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 padding2;

	Light gLights[LIGHT_MAX_COUNT];
};

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexCoord : TEXCOORD;
};

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float3 PositionW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexCoord : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);
	vout.PositionW = PositionW.xyz;
	vout.PositionH = mul(PositionW, gViewProj);

	vout.NormalW = mul(vin.NormalL, (float3x3)(gWorld));

	float4 TexCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), gTexCoordTransform);
	vout.TexCoord = mul(TexCoord, gMaterialTransform).xy;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 DiffuseAlbedo = gDiffuseTexture.Sample(gSamplerLinearWrap, pin.TexCoord) * gDiffuseAlbedo;

#if ALPHA_TEST
	clip(DiffuseAlbedo.a - 0.1f);
#endif // ALPHA_TEST

	pin.NormalW = normalize(pin.NormalW);

	float3 ToEyeW = gEyePositionW - pin.PositionW;
	float DistToEye = length(ToEyeW);
	ToEyeW /= DistToEye;

	// indirect lighting
	float4 ambient = gAmbientLight * DiffuseAlbedo;

	// direct lighting
	const float shininess = 1.0f - gRoughness;
	Material material = { DiffuseAlbedo, gFresnelR0, shininess };
	float3 ShadowFactor = 1.0f;
	float4 direct = ComputeLighting(gLights, material, pin.PositionW, pin.NormalW, ToEyeW, ShadowFactor);
	
	float4 result = ambient + direct;

#ifdef FOG
	float FogAmount = saturate((DistToEye - gFogStart) / gFogRange);
	result = lerp(result, gFogColor, FogAmount);
#endif // FOG

	result.a = DiffuseAlbedo.a;

	return result;
}