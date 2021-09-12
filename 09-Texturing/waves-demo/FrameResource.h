#pragma once

#include "utils.h"
#include "MathHelper.h"

#define LIGHT_MAX_COUNT 16

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 TexCoord;
};

struct ObjectConstants
{
	XMFLOAT4X4 world = MathHelper::Identity4x4();
	XMFLOAT4X4 TexCoordTransform = MathHelper::Identity4x4();
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
	float padding;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	XMFLOAT2 RenderTargetSizeInverse = { 0.0f, 0.0f };
	float NearPlane = 0;
	float FarPlane = 0;
	float DeltaTime = 0;
	float TotalTime = 0;

	XMFLOAT4 AmbientLight = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	Light lights[LIGHT_MAX_COUNT];
};

struct FrameResource
{
	FrameResource(ID3D12Device* device, UINT MainPassCount, UINT ObjectCount, UINT MaterialCount, UINT WaveVertexCount);
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;

	std::unique_ptr<UploadBuffer<MainPassConstants>> MainPassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;

	UINT64 fence = 0;
};