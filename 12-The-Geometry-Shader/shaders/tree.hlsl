#include "LightingUtils.hlsl"

Texture2DArray gTreeTextureArray : register(t0);
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
	float3 PositionW : POSITION;
	float2 SizeW : SIZE;
};

struct VertexOut
{
	float3 PositionW : POSITION;
	float2 SizeW : SIZE;
};

struct GeometryOut
{
	float4 PositionH : SV_POSITION;
	float3 PositionW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexCoord : TEXCOORD;
    uint   PrimitiveID : SV_PrimitiveID;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.PositionW = vin.PositionW;
	vout.SizeW = vin.SizeW;

	return vout;
}

[maxvertexcount(4)]
void GS(point VertexOut gin[1], uint PrimitiveID : SV_PrimitiveID, inout TriangleStream<GeometryOut> stream)
{
	GeometryOut gout;

    float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 look = gEyePositionW - gin[0].PositionW;
	look.y = 0.0f; // y-axis aligned, so project to xz-plane
	look = normalize(look);
	float3 right = cross(up, look);

	float2 HalfSize = 0.5f * gin[0].SizeW;

	float4 vertices[4];
	vertices[0] = float4(gin[0].PositionW + HalfSize.x*right - HalfSize.y*up, 1.0f);
	vertices[1] = float4(gin[0].PositionW + HalfSize.x*right + HalfSize.y*up, 1.0f);
	vertices[2] = float4(gin[0].PositionW - HalfSize.x*right - HalfSize.y*up, 1.0f);
	vertices[3] = float4(gin[0].PositionW - HalfSize.x*right + HalfSize.y*up, 1.0f);

    float2 TexCoord[4] = 
	{
		float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f),
	};
	
	[unroll]
	for(uint i = 0; i < 4; ++i)
	{
		gout.PositionH   = mul(vertices[i], gViewProj);
		gout.PositionW   = vertices[i].xyz;
		gout.NormalW     = look;
		gout.TexCoord    = TexCoord[i];
		gout.PrimitiveID = PrimitiveID;
		
		stream.Append(gout);
	}
}

float4 PS(GeometryOut pin) : SV_Target
{
    float3 TexCoord = float3(pin.TexCoord, pin.PrimitiveID % 3);
	float4 DiffuseAlbedo = gTreeTextureArray.Sample(gSamplerLinear, TexCoord) * gDiffuseAlbedo;

#if ALPHA_TEST
	clip(DiffuseAlbedo.a - 0.1f);
#endif // ALPHA_TEST

	pin.NormalW = normalize(pin.NormalW);

	float3 ToEyeW = gEyePositionW - pin.PositionW;
	float DistToEye = length(ToEyeW);
	ToEyeW /= DistToEye;

	// indirect lighting
	float4 ambient = gAmbientLight * DiffuseAlbedo;

	// direct lighting
	const float shininess = 1.0f - gRoughness;
	Material material = { DiffuseAlbedo, gFresnelR0, shininess };
	float3 ShadowFactor = 1.0f;
	float4 direct = ComputeLighting(gLights, material, pin.PositionW, pin.NormalW, ToEyeW, ShadowFactor);
	
	float4 result = ambient + direct;

#ifdef FOG
	float FogAmount = saturate((DistToEye - gFogStart) / gFogRange);
	result = lerp(result, gFogColor, FogAmount);
#endif // FOG

	result.a = DiffuseAlbedo.a;

	return result;
}