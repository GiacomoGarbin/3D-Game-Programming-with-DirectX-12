#pragma once

#include "utils.h"

class ShadowMap
{
	ID3D12Device* mDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;

	void BuildDescriptors();
	void BuildResource();

public:
	ShadowMap(ID3D12Device* device,
			  UINT width,
			  UINT height);

	UINT GetWidth() const;
	UINT GetHeight() const;
	ID3D12Resource* GetResource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSRV() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDSV() const;

	const D3D12_VIEWPORT& GetViewport() const;
	const D3D12_RECT& GetScissorRect() const;

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
						  CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
						  CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void OnResize(UINT width, UINT height);
};