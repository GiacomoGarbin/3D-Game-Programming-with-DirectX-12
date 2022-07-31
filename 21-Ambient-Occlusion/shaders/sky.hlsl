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
	float3 PositionL : POSITION;
};

VertexOut VS(const VertexIn vin)
{
	VertexOut vout;

    // use local position as cubemap lookup vector
    vout.PositionL = vin.PositionL.xyz;

	float4 PositionW = mul(float4(vin.PositionL, 1.0f), gWorld);
	
    // center sky about the camera
    PositionW.xyz += gEyePositionW;

	// set z = w so that z/w = 1 (i.e., skydome always on far plane)
	vout.PositionH = mul(PositionW, gViewProj).xyww;

	return vout;
}

float4 PS(const VertexOut pin) : SV_Target
{
	return gCubeMap.Sample(gSamplerLinearWrap, pin.PositionL);
}