#include "LightingUtils.hlsl"

Texture2D gDiffuseTexture : register(t0);
SamplerState gSamplerLinear : register(s0);

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
	float4 DiffuseAlbedo = gDiffuseTexture.Sample(gSamplerLinear, pin.TexCoord) * gDiffuseAlbedo;

#if ALPHA_TEST
	clip(DiffuseAlbedo.a - 0.1f);
#endif // ALPHA_TEST

	pin.NormalW = normalize(pin.NormalW);

	float3 ToEyeW = normalize(gEyePositionW - pin.PositionW);

	// indirect lighting
	float4 ambient = gAmbientLight * DiffuseAlbedo;

	// direct lighting
	const float shininess = 1.0f - gRoughness;
	Material material = { DiffuseAlbedo, gFresnelR0, shininess };
	float3 ShadowFactor = 1.0f;
	float4 direct = ComputeLighting(gLights, material, pin.PositionW, pin.NormalW, ToEyeW, ShadowFactor);
	
	float4 result = ambient + direct;
	result.a = DiffuseAlbedo.a;

	return result;
}