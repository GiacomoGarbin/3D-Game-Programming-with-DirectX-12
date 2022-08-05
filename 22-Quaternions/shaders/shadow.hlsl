#include "common.hlsl"

struct VertexIn
{
	float3 PositionL : POSITION;
	float2 TexCoord  : TEXCOORD;
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