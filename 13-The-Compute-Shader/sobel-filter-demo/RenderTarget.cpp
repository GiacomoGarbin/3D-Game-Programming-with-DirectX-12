#include "RenderTarget.h"

RenderTarget::RenderTarget(ID3D12Device* device,
						   UINT width,
						   UINT height,
						   DXGI_FORMAT format) :
	mDevice(device),
	mWidth(width),
	mHeight(height),
	mFormat(format)
{
	BuildResource();
}

void RenderTarget::OnResize(UINT width, UINT height)
{
	if ((width != mWidth) || (height != mHeight))
	{
		mWidth = width;
		mHeight = height;

		BuildResource();
		BuildDescriptors();
	}
}

ID3D12Resource* RenderTarget::GetResource()
{
	return mOffscreenTexture.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE RenderTarget::GetSRV()
{
	return mGpuSRV;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE RenderTarget::GetRTV()
{
	return mCpuRTV;
}

UINT RenderTarget::DescriptorCount() const
{
	return 1;
}

void RenderTarget::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE SrvCpuDescriptor,
									CD3DX12_GPU_DESCRIPTOR_HANDLE SrvGpuDescriptor,
									CD3DX12_CPU_DESCRIPTOR_HANDLE RtvCpuDescriptor)
{
	mCpuSRV = SrvCpuDescriptor;
	mGpuSRV = SrvGpuDescriptor;
	mCpuRTV = RtvCpuDescriptor;

	BuildDescriptors();
}

void RenderTarget::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Format = mFormat;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MostDetailedMip = 0;
	desc.Texture2D.MipLevels = 1;

	mDevice->CreateShaderResourceView(mOffscreenTexture.Get(), &desc, mCpuSRV);
	mDevice->CreateRenderTargetView(mOffscreenTexture.Get(), nullptr, mCpuRTV);
}

void RenderTarget::BuildResource()
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
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	FLOAT FogColor[4] = { 0.7f, 0.7f, 0.7f, 1.0f };

	D3D12_CLEAR_VALUE ClearValue;
	ClearValue.Format = mFormat;
	CopyMemory(&ClearValue.Color, FogColor, sizeof(FogColor));

	auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(mDevice->CreateCommittedResource(&heap,
												   D3D12_HEAP_FLAG_NONE,
												   &desc,
												   D3D12_RESOURCE_STATE_COPY_DEST,
												   &ClearValue,
												   IID_PPV_ARGS(&mOffscreenTexture)));

	mOffscreenTexture->SetName(L"Offscreen Texture");
}