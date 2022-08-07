#include "common.hlsl"

struct VertexIn
{
	float3 PositionL : POSITION;
	float2 TexCoord  : TEXCOORD;
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : INDICES;
#endif // SKINNED
};

struct VertexOut
{
	float4 PositionH : SV_POSITION;
	float2 TexCoord  : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	MaterialData material = gMaterialBuffer[gMaterialIndex];
	
#ifdef SKINNED
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

    float3 PositionL = float3(0.0f, 0.0f, 0.0f);

    for(int i = 0; i < 4; ++i)
    {
		const float4x4 BoneTransform = gBoneTransforms[vin.BoneIndices[i]];

        PositionL += weights[i] * mul(float4(vin.PositionL, 1.0f), BoneTransform).xyz;
    }

    vin.PositionL = PositionL;
#endif // SKINNED

    // transform to world space
	const float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);

    // transform to homogeneous clip space
    vout.PositionH = mul(PositionW, gViewProj);
	
	const float4 TexCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), gTexCoordTransform);
	vout.TexCoord = mul(TexCoord, material.transform).xy;
	
    return vout;
}

// only used for alpha cut out geometry
// geometry that does not need to sample a texture can use a NULL pixel shader for depth pass
void PS(VertexOut pin) 
{
	const MaterialData material = gMaterialBuffer[gMaterialIndex];

	const float4 DiffuseAlbedo = gDiffuseTexture[material.DiffuseTextureIndex].Sample(gSamplerLinearWrap, pin.TexCoord) * material.DiffuseAlbedo;

#if ALPHA_TEST
	clip(DiffuseAlbedo.a - 0.1f);
#endif // ALPHA_TEST
}