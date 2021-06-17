#pragma once

#include "utils.h"
#include "MathHelper.h"

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

struct ObjectConstants
{
	XMFLOAT4X4 world = MathHelper::Identity4x4();
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
};

struct FrameResource
{
	FrameResource(ID3D12Device* device, UINT PassCount, UINT ObjectCount, UINT WaveVertexCount);
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;

	std::unique_ptr<UploadBuffer<MainPassConstants>> MainPassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;

	UINT64 fence = 0;
};