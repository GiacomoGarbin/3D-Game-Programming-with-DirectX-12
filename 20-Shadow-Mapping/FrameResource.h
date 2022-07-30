#pragma once

#include "utils.h"
#include "MathHelper.h"

#define LIGHT_MAX_COUNT 16
#define IS_FOG_ENABLED 0

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 TexCoord;
	XMFLOAT3 tangent;
};

struct ObjectConstants
{
	XMFLOAT4X4 world = MathHelper::Identity4x4();
	XMFLOAT4X4 TexCoordTransform = MathHelper::Identity4x4();
	UINT MaterialIndex = -1;
	XMFLOAT3 padding;
};

struct MainPassConstants
{
	XMFLOAT4X4 view = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewInverse = MathHelper::Identity4x4();
	XMFLOAT4X4 proj = MathHelper::Identity4x4();
	XMFLOAT4X4 ProjInverse = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProjInverse = MathHelper::Identity4x4();
	XMFLOAT3 EyePositionWorld = { 0.0f, 0.0f, 0.0f };
	float padding1;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	XMFLOAT2 RenderTargetSizeInverse = { 0.0f, 0.0f };
	float NearPlane = 0.0f;
	float FarPlane = 0.0f;
	float DeltaTime = 0.0f;
	float TotalTime = 0.0f;

	XMFLOAT4 AmbientLight = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

#if IS_FOG_ENABLED
	XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float FogStart = 5.0f;
	float FogRange = 150.0f;
	XMFLOAT2 padding2;
#endif // IS_FOG_ENABLED

	Light lights[LIGHT_MAX_COUNT];
};

struct MaterialData
{
	XMFLOAT4 DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMFLOAT3 FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	float roughness = 64.0f;

	// used in texture mapping
	XMFLOAT4X4 transform = MathHelper::Identity4x4();

	UINT DiffuseTextureIndex = 0;
	UINT NormalTextureIndex = 0;
	XMFLOAT2 padding;
};

struct FrameResource
{
	FrameResource(ID3D12Device* device,
				  const UINT MainPassCount,
				  const UINT ObjectCount,
				  const UINT MaterialCount);
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;

	std::unique_ptr<UploadBuffer<MainPassConstants>> MainPassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	UINT64 fence = 0;
};