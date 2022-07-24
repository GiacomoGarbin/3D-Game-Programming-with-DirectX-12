#pragma once

#include "utils.h"

class CubeRenderTarget
{
	ID3D12Device* mDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuRtv[6];

	Microsoft::WRL::ComPtr<ID3D12Resource> mCubeMap = nullptr;

	void BuildDescriptors();
	void BuildResource();

public:
	CubeRenderTarget(ID3D12Device* device,
					 const int width,
					 const int height,
					 const DXGI_FORMAT format);

	ID3D12Resource* GetResource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSRV();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTV(const UINT index);

	const D3D12_VIEWPORT& GetViewport() const;
	const D3D12_RECT& GetScissorRect() const;

	void BuildDescriptors(const CD3DX12_CPU_DESCRIPTOR_HANDLE& CpuSrv,
						  const CD3DX12_GPU_DESCRIPTOR_HANDLE& GpuSrv,
						  const CD3DX12_CPU_DESCRIPTOR_HANDLE CpuRtv[6]);

	void OnResize(const UINT width, const UINT height);
};

