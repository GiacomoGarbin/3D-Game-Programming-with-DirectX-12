#include "ShadowMap.h"


ShadowMap::ShadowMap(ID3D12Device* device,
					 UINT width,
					 UINT height)
{
	mDevice = device;

	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, static_cast<FLOAT>(width), static_cast<FLOAT>(height), 0.0f, 1.0f};
	mScissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

	BuildResource();
}

UINT ShadowMap::GetWidth() const
{
	return mWidth;
}

UINT ShadowMap::GetHeight() const
{
	return mHeight;
}

ID3D12Resource* ShadowMap::GetResource()
{
	return mShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::GetSRV() const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::GetDSV() const
{
	return mhCpuDsv;
}

const D3D12_VIEWPORT& ShadowMap::GetViewport() const
{
	return mViewport;
}

const D3D12_RECT& ShadowMap::GetScissorRect() const
{
	return mScissorRect;
}

void ShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
								 CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
								 CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	// cache references to the descriptors
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuDsv = hCpuDsv;

	BuildDescriptors();
}

void ShadowMap::OnResize(UINT width, UINT height)
{
	if ((mWidth != width) || (mHeight != height))
	{
		mWidth = width;
		mHeight = height;

		BuildResource();
		BuildDescriptors();
	}
}

void ShadowMap::BuildDescriptors()
{
	// SRV
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = 1;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;
		desc.Texture2D.PlaneSlice = 0;

		mDevice->CreateShaderResourceView(mShadowMap.Get(), &desc, mhCpuSrv);
	}

	// DSV
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Flags = D3D12_DSV_FLAG_NONE;
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.Texture2D.MipSlice = 0;

		mDevice->CreateDepthStencilView(mShadowMap.Get(), &desc, mhCpuDsv);
	}
}

void ShadowMap::BuildResource()
{
	D3D12_RESOURCE_DESC desc;
	ZeroMemory(&desc, sizeof(D3D12_RESOURCE_DESC));
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = mWidth;
	desc.Height = mHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = mFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clear;
	clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clear.DepthStencil.Depth = 1.0f;
	clear.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES type(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(mDevice->CreateCommittedResource(&type,
												   D3D12_HEAP_FLAG_NONE,
												   &desc,
												   D3D12_RESOURCE_STATE_GENERIC_READ,
												   &clear,
												   IID_PPV_ARGS(&mShadowMap)));

	const std::string name = "ShadowMap";
	ThrowIfFailed(mShadowMap->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.data()));
}