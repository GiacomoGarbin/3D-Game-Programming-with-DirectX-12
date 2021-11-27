#pragma once

#include "utils.h"

class Sobel
{
	ID3D12Device* mDevice = nullptr;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSRV;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuUAV;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuUAV;

	Microsoft::WRL::ComPtr<ID3D12Resource> mOutputTexture = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	void BuildResource();
	void BuildDescriptors();

public:
	Sobel(ID3D12Device* device,
				 UINT width,
				 UINT height,
				 DXGI_FORMAT format);

	void OnResize(UINT width, UINT height);

	void execute(ID3D12GraphicsCommandList* CommandList,
				 ID3D12PipelineState* PSO,
				 CD3DX12_GPU_DESCRIPTOR_HANDLE input);

	CD3DX12_GPU_DESCRIPTOR_HANDLE output();
	ID3D12Resource* GetResource();

	void BuildRootSignature(std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>& StaticSamplers);
	ID3D12RootSignature* GetRootSignature();

	UINT DescriptorCount() const;
	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptor,
						  CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptor,
						  UINT DescriptorSize);
};