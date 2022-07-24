#include "common.hlsl"

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

VertexOut VS(const VertexIn vin)
{
	VertexOut vout;

	const MaterialData material = gMaterialBuffer[gMaterialIndex];

	const float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);
	vout.PositionW = PositionW.xyz;
	vout.PositionH = mul(PositionW, gViewProj);

	vout.NormalW = mul(vin.NormalL, (float3x3)(gWorld));

	const float4 TexCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), gTexCoordTransform);
	vout.TexCoord = mul(TexCoord, material.transform).xy;

	return vout;
}

float4 PS(const VertexOut pin) : SV_Target
{
	const MaterialData material = gMaterialBuffer[gMaterialIndex];

	const float4 DiffuseAlbedo = gDiffuseTexture[material.DiffuseTextureIndex].Sample(gSamplerLinearWrap, pin.TexCoord) * material.DiffuseAlbedo;

#if ALPHA_TEST
	clip(DiffuseAlbedo.a - 0.1f);
#endif // ALPHA_TEST

	const float3 normal = normalize(pin.NormalW);

	float3 ToEyeW = gEyePositionW - pin.PositionW;
	const float DistToEye = length(ToEyeW);
	ToEyeW /= DistToEye;

	// indirect lighting
	const float4 ambient = gAmbientLight * DiffuseAlbedo;

	// direct lighting
	const float shininess = 1.0f - material.roughness;
	const Material LightMaterial = { DiffuseAlbedo, material.FresnelR0, shininess };
	const float3 ShadowFactor = 1.0f;
	const float4 direct = ComputeLighting(gLights, LightMaterial, pin.PositionW, normal, ToEyeW, ShadowFactor);
	
	float4 result = ambient + direct;

#ifdef FOG
	const float FogAmount = saturate((DistToEye - gFogStart) / gFogRange);
	result = lerp(result, gFogColor, FogAmount);
#endif // FOG

	result.a = DiffuseAlbedo.a;

	return result;
}