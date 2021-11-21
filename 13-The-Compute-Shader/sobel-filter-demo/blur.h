#pragma once

#include "utils.h"

class Blur
{
	ID3D12Device* mDevice = nullptr;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv0;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuUav0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuUav1;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuUav0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv1;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuUav1;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap0 = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap1 = nullptr;

	void BuildResources();
	void BuildDescriptors();

	std::vector<float> CalcGaussWeights(float sigma);

public:
	Blur(ID3D12Device* device,
		 UINT width,
		 UINT height,
		 DXGI_FORMAT format);

	void OnResize(UINT width, UINT height);
	
	void execute(ID3D12GraphicsCommandList* CommandList,
				 ID3D12RootSignature* RootSignature,
				 ID3D12PipelineState* HorzBlurPSO,
				 ID3D12PipelineState* VertBlurPSO,
				 ID3D12Resource* input,
				 int count);
	
	ID3D12Resource* output();

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE CPUDescriptor,
						  CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptor,
						  UINT DescriptorSize);
};