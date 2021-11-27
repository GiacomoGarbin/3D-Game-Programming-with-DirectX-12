#include "sobel.h"

Sobel::Sobel(ID3D12Device* device,
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

void Sobel::OnResize(UINT width, UINT height)
{
	if ((width != mWidth) || (height != mHeight))
	{
		mWidth = width;
		mHeight = height;

		BuildResource();
		BuildDescriptors();
	}
}

void Sobel::execute(ID3D12GraphicsCommandList* CommandList,
					ID3D12PipelineState* PSO,
					CD3DX12_GPU_DESCRIPTOR_HANDLE input)
{
	CommandList->SetComputeRootSignature(mRootSignature.Get());
	CommandList->SetPipelineState(PSO);

	CommandList->SetComputeRootDescriptorTable(0, input);
	CommandList->SetComputeRootDescriptorTable(2, mGpuUAV); // <-- ?

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mOutputTexture.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		CommandList->ResourceBarrier(1, &transition);
	}

	UINT x = std::ceil(mWidth / 16.0f);
	UINT y = std::ceil(mHeight / 16.0f);
	CommandList->Dispatch(x, y, 1);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mOutputTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList->ResourceBarrier(1, &transition);
	}
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Sobel::output()
{
	return mGpuSRV;
}

ID3D12Resource* Sobel::GetResource()
{
	return mOutputTexture.Get();
}

void Sobel::BuildRootSignature(std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>& StaticSamplers)
{
	CD3DX12_DESCRIPTOR_RANGE SRV0;
	SRV0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	
	CD3DX12_DESCRIPTOR_RANGE SRV1;
	SRV1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE UAV0;
	UAV0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER params[3];
	params[0].InitAsDescriptorTable(1, &SRV0);
	params[1].InitAsDescriptorTable(1, &SRV1);
	params[2].InitAsDescriptorTable(1, &UAV0);

	CD3DX12_ROOT_SIGNATURE_DESC desc(3,
									 params,
									 1,
									 &StaticSamplers[1], // point clamp
									 //StaticSamplers.size(),
									 //StaticSamplers.data(),
									 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> signature = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> error = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&desc,
											 D3D_ROOT_SIGNATURE_VERSION_1,
											 signature.GetAddressOf(),
											 error.GetAddressOf());

	if (error != nullptr)
	{
		::OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
	}

	ThrowIfFailed(hr);

	ThrowIfFailed(mDevice->CreateRootSignature(0,
											   signature->GetBufferPointer(),
											   signature->GetBufferSize(),
											   IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

ID3D12RootSignature* Sobel::GetRootSignature()
{
	return mRootSignature.Get();
}

UINT Sobel::DescriptorCount() const
{
	return 2;
}

void Sobel::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE CpuDescriptor,
							 CD3DX12_GPU_DESCRIPTOR_HANDLE GpuDescriptor,
							 UINT DescriptorSize)
{
	mCpuSRV = CpuDescriptor;
	mCpuUAV = CpuDescriptor.Offset(1, DescriptorSize);
	mGpuSRV = GpuDescriptor;
	mGpuUAV = GpuDescriptor.Offset(1, DescriptorSize);

	BuildDescriptors();
}

void Sobel::BuildDescriptors()
{
	// SRV
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Format = mFormat;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = 1;

		mDevice->CreateShaderResourceView(mOutputTexture.Get(), &desc, mCpuSRV);
	}

	// UAV
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
		desc.Format = mFormat;
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;

		mDevice->CreateUnorderedAccessView(mOutputTexture.Get(), nullptr, &desc, mCpuUAV);
	}
}

void Sobel::BuildResource()
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
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(mDevice->CreateCommittedResource(&heap,
												   D3D12_HEAP_FLAG_NONE,
												   &desc,
												   D3D12_RESOURCE_STATE_GENERIC_READ,
												   nullptr,
												   IID_PPV_ARGS(&mOutputTexture)));

	mOutputTexture->SetName(L"Sober Output Texture");
}