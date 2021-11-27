#pragma once

#include "utils.h"

class RenderTarget
{
	ID3D12Device* mDevice = nullptr;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuRTV;

	Microsoft::WRL::ComPtr<ID3D12Resource> mOffscreenTexture = nullptr;

	void BuildResource();
	void BuildDescriptors();

public:
	RenderTarget(ID3D12Device* device,
				 UINT width,
				 UINT height,
				 DXGI_FORMAT format);

	void OnResize(UINT width, UINT height);

	ID3D12Resource* GetResource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSRV();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTV();

	UINT DescriptorCount() const;
	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCpuDescriptor,
						  CD3DX12_GPU_DESCRIPTOR_HANDLE SrvGpuDescriptor,
						  CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCpuDescriptor);
};