#include "LightingUtils.hlsl"

struct InstanceData
{
	float4x4 world;
	float4x4 TexCoordTransform;
	uint MaterialIndex;
	float3 padding;
};

struct MaterialData
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float  roughness;
	float4x4 transform;
	uint DiffuseTextureIndex;
	float3 padding;
};

// array of textures
Texture2D gDiffuseTexture[7] : register(t0, space0);
// instance buffer, it contains all visible instances
StructuredBuffer<InstanceData> gInstanceBuffer : register(t0, space1);
// material buffer, it contains all materials
StructuredBuffer<MaterialData> gMaterialBuffer : register(t1, space1);

SamplerState gSamplerLinearWrap : register(s2);

cbuffer MainPassCB : register(b0)
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

#ifdef FOG
	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 padding2;
#endif // FOG

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

	nointerpolation uint MaterialIndex : MATERIAL_INDEX;
};

VertexOut VS(const VertexIn vin, const uint InstanceID : SV_InstanceID)
{
	VertexOut vout;

	const InstanceData instance = gInstanceBuffer[InstanceID];
	const MaterialData material = gMaterialBuffer[instance.MaterialIndex];

	const float4 PositionW = mul(float4(vin.PositionL, 1.0f), instance.world);
	vout.PositionW = PositionW.xyz;
	vout.PositionH = mul(PositionW, gViewProj);

	vout.NormalW = mul(vin.NormalL, (float3x3)(instance.world));

	const float4 TexCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), instance.TexCoordTransform);
	vout.TexCoord = mul(TexCoord, material.transform).xy;

	vout.MaterialIndex = instance.MaterialIndex;

	return vout;
}

float4 PS(const VertexOut pin) : SV_Target
{
	const MaterialData material = gMaterialBuffer[pin.MaterialIndex];

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