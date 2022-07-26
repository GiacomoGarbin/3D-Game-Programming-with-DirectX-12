#include "LightingUtils.hlsl"

struct MaterialData
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float  roughness;
	float4x4 transform;
	uint DiffuseTextureIndex;
	float3 padding;
};

// sky cube map texture
TextureCube gCubeMap : register(t0, space0);
// array of textures
Texture2D gDiffuseTexture[3] : register(t1, space0);
// material buffer, it contains all materials
StructuredBuffer<MaterialData> gMaterialBuffer : register(t0, space1);

SamplerState gSamplerLinearWrap : register(s2);

cbuffer ObjectCB : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexCoordTransform;
	uint gMaterialIndex;
	float3 padding;
};

cbuffer MainPassCB : register(b1)
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