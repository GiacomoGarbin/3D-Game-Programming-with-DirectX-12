#include "CubeRenderTarget.h"

CubeRenderTarget::CubeRenderTarget(ID3D12Device* device,
								   const int width,
								   const int height,
								   const DXGI_FORMAT format)
{
	mDevice = device;

	mWidth = width;
	mHeight = height;
	mFormat = format;

	mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
	mScissorRect = { 0, 0, width, height };

	BuildResource();
}

ID3D12Resource* CubeRenderTarget::GetResource()
{
	return mCubeMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeRenderTarget::GetSRV()
{
	return mGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::GetRTV(const UINT index)
{
	return mCpuRtv[index];
}

const D3D12_VIEWPORT& CubeRenderTarget::GetViewport() const
{
	return mViewport;
}

const D3D12_RECT& CubeRenderTarget::GetScissorRect() const
{
	return mScissorRect;
}

void CubeRenderTarget::BuildDescriptors(const CD3DX12_CPU_DESCRIPTOR_HANDLE& CpuSrv,
										const CD3DX12_GPU_DESCRIPTOR_HANDLE& GpuSrv,
										const CD3DX12_CPU_DESCRIPTOR_HANDLE CpuRtv[6])
{
	// cache descriptors
	{
		mCpuSrv = CpuSrv;
		mGpuSrv = GpuSrv;

		for (UINT i = 0; i < 6; ++i)
		{
			mCpuRtv[i] = CpuRtv[i];
		}
	}

	BuildDescriptors();
}

void CubeRenderTarget::OnResize(const UINT width, const UINT height)
{
	if ((mWidth != width) || (mHeight != height))
	{
		mWidth = width;
		mHeight = height;

		BuildResource();
		BuildDescriptors();
	}
}

void CubeRenderTarget::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Format = mFormat;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	desc.TextureCube.MostDetailedMip = 0;
	desc.TextureCube.MipLevels = 1;
	desc.TextureCube.ResourceMinLODClamp = 0.0f;

	// create SRV to the entire cubemap resource
	mDevice->CreateShaderResourceView(mCubeMap.Get(), &desc, mCpuSrv);

	// create RTV to each cubemap face
	for (UINT i = 0; i < 6; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC desc;
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		desc.Format = mFormat;
		desc.Texture2DArray.MipSlice = 0;
		desc.Texture2DArray.PlaneSlice = 0;
		desc.Texture2DArray.FirstArraySlice = i;
		desc.Texture2DArray.ArraySize = 1;

		// create RTV to ith cubemap face
		mDevice->CreateRenderTargetView(mCubeMap.Get(), &desc, mCpuRtv[i]);
	}
}

void CubeRenderTarget::BuildResource()
{
	D3D12_RESOURCE_DESC desc;
	ZeroMemory(&desc, sizeof(D3D12_RESOURCE_DESC));
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = mWidth;
	desc.Height = mHeight;
	desc.DepthOrArraySize = 6;
	desc.MipLevels = 1;
	desc.Format = mFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clear;
	clear.Format = mFormat;
	std::memcpy(clear.Color, Colors::LightSteelBlue, sizeof(clear.Color));

	const auto properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(mDevice->CreateCommittedResource(&properties,
												   D3D12_HEAP_FLAG_NONE,
												   &desc,
												   D3D12_RESOURCE_STATE_GENERIC_READ,
												   &clear,
												   IID_PPV_ARGS(&mCubeMap)));
}