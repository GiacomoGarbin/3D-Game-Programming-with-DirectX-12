#include "common.hlsl"

struct VertexIn
{
	float3 PositionL : POSITION;
	float3 NormalL : NORMAL;
	float2 TexCoord : TEXCOORD;
	float3 TangentL : TANGENT;
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : INDICES;
#endif // SKINNED
};

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float3 NormalW : NORMAL;
	float3 TangentW : TANGENT;
	float2 TexCoord : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	const MaterialData material = gMaterialBuffer[gMaterialIndex];

#ifdef SKINNED
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

    float3 PositionL = float3(0.0f, 0.0f, 0.0f);
    float3 NormalL = float3(0.0f, 0.0f, 0.0f);
    float3 TangentL = float3(0.0f, 0.0f, 0.0f);

    for(int i = 0; i < 4; ++i)
    {
		const float4x4 BoneTransform = gBoneTransforms[vin.BoneIndices[i]];

        PositionL += weights[i] * mul(float4(vin.PositionL, 1.0f), BoneTransform).xyz;
        NormalL += weights[i] * mul(vin.NormalL, (float3x3)(BoneTransform));
        TangentL += weights[i] * mul(vin.TangentL.xyz, (float3x3)(BoneTransform));
    }

    vin.PositionL = PositionL;
    vin.NormalL = NormalL;
    vin.TangentL.xyz = TangentL;
#endif // SKINNED

	const float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);
	vout.PositionH = mul(PositionW, gViewProj);

	vout.NormalW = mul(vin.NormalL, (float3x3)(gWorld));
	vout.TangentW = mul(vin.TangentL, (float3x3)(gWorld));

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

#if NORMAL_MAPPING
	const float4 NormalSample = gDiffuseTexture[material.NormalTextureIndex].Sample(gSamplerLinearWrap, pin.TexCoord);
	const float3 normal = NormalSampleToWorldSpace(NormalSample.rgb, normalize(pin.NormalW), pin.TangentW);
#else // NORMAL_MAPPING
	const float3 normal = normalize(pin.NormalW);
#endif // NORMAL_MAPPING

	return float4(mul(normal, (float3x3)(gView)), 0.0f);
}