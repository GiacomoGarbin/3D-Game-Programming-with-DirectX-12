#include "LightingUtils.hlsl"

// Texture2D gDiffuseTexture : register(t0);
// SamplerState gSamplerLinearWrap : register(s2);

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
};

struct VertexOut
{
	float3 PositionL : POSITION;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

    vout.PositionL = vin.PositionL;

	return vout;
}

struct PatchTess
{
	float EdgeTessFactor[4]   : SV_TessFactor;
	float InsideTessFactor[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, const uint PatchID : SV_PrimitiveID)
{
	PatchTess pt;
	
	const float3 centerL = 0.25f * (patch[0].PositionL + patch[1].PositionL + patch[2].PositionL + patch[3].PositionL);
	const float3 centerW = mul(float4(centerL, 1.0f), gWorld).xyz;
	
	const float dx = distance(centerW, gEyePositionW);

	const float d0 = 20.0f;
	const float d1 = 100.0f;
	const float TessFactor = 64.0f * saturate((d1-dx)/(d1-d0));

	pt.EdgeTessFactor[0] = TessFactor;
	pt.EdgeTessFactor[1] = TessFactor;
	pt.EdgeTessFactor[2] = TessFactor;
	pt.EdgeTessFactor[3] = TessFactor;
	
	pt.InsideTessFactor[0] = TessFactor;
	pt.InsideTessFactor[1] = TessFactor;
	
	return pt;
}

struct HullOut
{
	float3 PositionL : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 4> pt, 
           const uint i : SV_OutputControlPointID,
           const uint PatchId : SV_PrimitiveID)
{
	HullOut hout;
	
	hout.PositionL = pt[i].PositionL;
	
	return hout;
}

struct DomainOut
{
	float4 PositionH : SV_POSITION;
};

[domain("quad")]
DomainOut DS(PatchTess pt, 
             const float2 uv : SV_DomainLocation, 
             const OutputPatch<HullOut, 4> quad)
{
	DomainOut dout;
	
	// bilinear interpolation
	const float3 v1 = lerp(quad[0].PositionL, quad[1].PositionL, uv.x); 
	const float3 v2 = lerp(quad[2].PositionL, quad[3].PositionL, uv.x); 
	float3 position  = lerp(v1, v2, uv.y); 
	
	// displacement mapping
	position.y = 0.3f * (position.z * sin(position.x) + position.x * cos(position.z));
	
	const float4 PositionH = mul(float4(position, 1.0f), gWorld);
	dout.PositionH = mul(PositionH, gViewProj);
	
	return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}