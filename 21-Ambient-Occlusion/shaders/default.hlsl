#include "common.hlsl"

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexCoord : TEXCOORD;
	float3 TangentL : TANGENT;
};

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float3 PositionW : POSITION0;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float2 TexCoord : TEXCOORD;
	float4 ShadowPositionH : POSITION1;
};

VertexOut VS(const VertexIn vin)
{
	VertexOut vout;

	const MaterialData material = gMaterialBuffer[gMaterialIndex];

	const float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);
	vout.PositionW = PositionW.xyz;
	vout.PositionH = mul(PositionW, gViewProj);

	vout.NormalW = mul(vin.NormalL, (float3x3)(gWorld));
	vout.TangentW = mul(vin.TangentL, (float3x3)(gWorld));

	const float4 TexCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), gTexCoordTransform);
	vout.TexCoord = mul(TexCoord, material.transform).xy;

	// projective tex-coords to project shadow map onto scene
	vout.ShadowPositionH = mul(PositionW, gShadowMapTransform);

	return vout;
}

float4 PS(const VertexOut pin) : SV_Target
{
	const MaterialData material = gMaterialBuffer[gMaterialIndex];

	const float4 DiffuseAlbedo = gDiffuseTexture[material.DiffuseTextureIndex].Sample(gSamplerLinearWrap, pin.TexCoord) * material.DiffuseAlbedo;

#if ALPHA_TEST
	clip(DiffuseAlbedo.a - 0.1f);
#endif // ALPHA_TEST

#if NORMAL_MAPPING
	const float4 NormalSample = gDiffuseTexture[material.NormalTextureIndex].Sample(gSamplerLinearWrap, pin.TexCoord);
	const float3 normal = NormalSampleToWorldSpace(NormalSample.rgb, normalize(pin.NormalW), pin.TangentW);
#else // NORMAL_MAPPING
	const float3 normal = normalize(pin.NormalW);
#endif // NORMAL_MAPPING

	float3 ToEyeW = gEyePositionW - pin.PositionW;
	const float DistToEye = length(ToEyeW);
	ToEyeW /= DistToEye;

	// indirect lighting
#if AMBIENT_OCCLUSION || 1
	const float2 TexCoord = pin.PositionH.xy * gRenderTargetSizeInverse;
	const float AmbientAccess = gAmbientOcclusionMap.Sample(gSamplerLinearClamp, TexCoord, 0.0f).r;
	const float4 ambient = gAmbientLight * DiffuseAlbedo * AmbientAccess;
#else // AMBIENT_OCCLUSION
	const float4 ambient = gAmbientLight * DiffuseAlbedo;
#endif // AMBIENT_OCCLUSION
	
	// direct lighting
#if NORMAL_MAPPING
	const float shininess = (1.0f - material.roughness) * NormalSample.a;
#else // NORMAL_MAPPING
	const float shininess = 1.0f - material.roughness;
#endif // NORMAL_MAPPING
	const Material LightMaterial = { DiffuseAlbedo, material.FresnelR0, shininess };
#if SHADOW || 1
    // only the first light casts a shadow
    float3 ShadowFactor = float3(1.0f, 1.0f, 1.0f);
	ShadowFactor[0] = CalculateShadowFactor(pin.ShadowPositionH);
#else // SHADOW
	const float3 ShadowFactor = 1.0f;
#endif // SHADOW
	const float4 direct = ComputeLighting(gLights, LightMaterial, pin.PositionW, normal, ToEyeW, ShadowFactor);
	
	float4 result = ambient + direct;

	// specular reflections
	{
		const float3 r = reflect(-ToEyeW, normal);
		const float4 reflection = gCubeMap.Sample(gSamplerLinearWrap, r);
		const float3 fresnel = SchlickFresnel(material.FresnelR0, normal, r);
		result.rgb += shininess * fresnel * reflection.rgb;
	}

#if FOG
	const float FogAmount = saturate((DistToEye - gFogStart) / gFogRange);
	result = lerp(result, gFogColor, FogAmount);
#endif // FOG

	result.a = DiffuseAlbedo.a;

	return result;
}