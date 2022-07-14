#include "LightingUtils.hlsl"

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

PatchTess ConstantHS(InputPatch<VertexOut, 16> patch, const uint PatchID : SV_PrimitiveID)
{
	PatchTess pt;

	const float TessFactor = 25.0f;

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
[outputcontrolpoints(16)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 16> pt, 
           const uint i : SV_OutputControlPointID,
           const uint PatchID : SV_PrimitiveID)
{
	HullOut hout;
	
	hout.PositionL = pt[i].PositionL;
	
	return hout;
}

struct DomainOut
{
	float4 PositionH : SV_POSITION;
};

float4 BernsteinBasis(const float t)
{
    const float T = 1.0f - t;
	const float t2 = t * t;
	const float T2 = T * T;

    return float4(T2 * T,
                  3.0f * t * T2,
                  3.0f * t2 * T,
                  t2 * t);
}

float4 dBernsteinBasis(const float t)
{
    const float T = 1.0f - t;
	const float t2 = t * t;
	const float T2 = T * T;

    return float4(-3 * T2,
                  +3 * T2 - 6 * t * T,
                  +6 * t * T - 3 * t2,
                  +3 * t2);
}

float3 CubicBezierSum(const OutputPatch<HullOut, 16> patch, const float4 u, const float4 v)
{
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    sum  =  v.x * (u.x*patch[0x0].PositionL + u.y*patch[0x1].PositionL + u.z*patch[0x2].PositionL + u.w*patch[0x3].PositionL);
    sum +=  v.y * (u.x*patch[0x4].PositionL + u.y*patch[0x5].PositionL + u.z*patch[0x6].PositionL + u.w*patch[0x7].PositionL);
    sum +=  v.z * (u.x*patch[0x8].PositionL + u.y*patch[0x9].PositionL + u.z*patch[0xa].PositionL + u.w*patch[0xb].PositionL);
    sum +=  v.w * (u.x*patch[0xc].PositionL + u.y*patch[0xd].PositionL + u.z*patch[0xe].PositionL + u.w*patch[0xf].PositionL);
    return sum;
}

[domain("quad")]
DomainOut DS(PatchTess pt, 
             const float2 uv : SV_DomainLocation, 
             const OutputPatch<HullOut, 16> patch)
{
	DomainOut dout;
	
	const float4 u = BernsteinBasis(uv.x);
	const float4 v = BernsteinBasis(uv.y);	
	
	const float3 position = CubicBezierSum(patch, u, v);

	const float4 PositionW = mul(float4(position, 1.0f), gWorld);
	dout.PositionH = mul(PositionW, gViewProj);
	
	return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}