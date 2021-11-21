#include "blur.h"

Blur::Blur(ID3D12Device* device,
		   UINT width,
		   UINT height,
		   DXGI_FORMAT format) :
	mDevice(device),
	mWidth(width),
	mHeight(height),
	mFormat(format)
{
	BuildResources();
}

void Blur::BuildResources()
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

	ThrowIfFailed(mDevice->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBlurMap0)));

	mBlurMap0->SetName(L"BlurMap0");
	
	ThrowIfFailed(mDevice->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBlurMap1)));

	mBlurMap1->SetName(L"BlurMap1");
}

void Blur::OnResize(UINT width, UINT height)
{
	if ((width != mWidth) || (height != mHeight))
	{
		mWidth = width;
		mHeight = height;

		BuildResources();
		BuildDescriptors();
	}
}

void Blur::BuildDescriptors()
{
	// SRVs
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Format = mFormat;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.MipLevels = 1;

		mDevice->CreateShaderResourceView(mBlurMap0.Get(), &desc, mCpuSrv0);
		mDevice->CreateShaderResourceView(mBlurMap1.Get(), &desc, mCpuSrv1);
	}

	// UAVs
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
		desc.Format = mFormat;
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;

		mDevice->CreateUnorderedAccessView(mBlurMap0.Get(), nullptr, &desc, mCpuUav0);
		mDevice->CreateUnorderedAccessView(mBlurMap1.Get(), nullptr, &desc, mCpuUav1);
	}
}

void Blur::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE CPUDescriptor,
							CD3DX12_GPU_DESCRIPTOR_HANDLE GPUDescriptor,
							UINT DescriptorSize)
{
	mCpuSrv0 = CPUDescriptor;
	mCpuUav0 = CPUDescriptor.Offset(1, DescriptorSize);
	mCpuSrv1 = CPUDescriptor.Offset(1, DescriptorSize);
	mCpuUav1 = CPUDescriptor.Offset(1, DescriptorSize);

	mGpuSrv0 = GPUDescriptor;
	mGpuUav0 = GPUDescriptor.Offset(1, DescriptorSize);
	mGpuSrv1 = GPUDescriptor.Offset(1, DescriptorSize);
	mGpuUav1 = GPUDescriptor.Offset(1, DescriptorSize);

	BuildDescriptors();
}

void Blur::execute(ID3D12GraphicsCommandList* CommandList,
				   ID3D12RootSignature* RootSignature,
				   ID3D12PipelineState* HorzBlurPSO,
				   ID3D12PipelineState* VertBlurPSO,
				   ID3D12Resource* input,
				   int count)
{
	std::vector<float> weights = CalcGaussWeights(2.5f);
	INT radius = weights.size() / 2;

	CommandList->SetComputeRootSignature(RootSignature);

	CommandList->SetComputeRoot32BitConstants(0, 1, &radius, 0);
	CommandList->SetComputeRoot32BitConstants(0, (UINT)weights.size(), weights.data(), 1);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(input, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
		CommandList->ResourceBarrier(1, &transition);
	}

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		CommandList->ResourceBarrier(1, &transition);
	}

	// copy input (back buffer) to mBlurMap0
	CommandList->CopyResource(mBlurMap0.Get(), input);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		CommandList->ResourceBarrier(1, &transition);
	}

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		CommandList->ResourceBarrier(1, &transition);
	}

	for (UINT i = 0; i < count; ++i)
	{
		// horizontal blur
		{
			CommandList->SetPipelineState(HorzBlurPSO);

			CommandList->SetComputeRootDescriptorTable(1, mGpuSrv0);
			CommandList->SetComputeRootDescriptorTable(2, mGpuUav1);

			UINT x = std::ceil(mWidth / 256.0f);
			CommandList->Dispatch(x, mHeight, 1);

			{
				CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				CommandList->ResourceBarrier(1, &transition);
			}

			{
				CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
				CommandList->ResourceBarrier(1, &transition);
			}
		}

		// vertical blur
		{
			CommandList->SetPipelineState(VertBlurPSO);

			CommandList->SetComputeRootDescriptorTable(1, mGpuSrv1);
			CommandList->SetComputeRootDescriptorTable(2, mGpuUav0);

			UINT y = std::ceil(mHeight / 256.0f);
			CommandList->Dispatch(mWidth, y, 1);

			{
				CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
				CommandList->ResourceBarrier(1, &transition);
			}

			{
				CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				CommandList->ResourceBarrier(1, &transition);
			}
		}
	}

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON);
		CommandList->ResourceBarrier(1, &transition);
	}

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
		CommandList->ResourceBarrier(1, &transition);
	}
}

std::vector<float> Blur::CalcGaussWeights(float sigma)
{
	float TwoSigmaSquare = 2.0f * sigma*sigma;

	INT radius = std::ceil(2.0f * sigma);

	assert(radius <= 5); // max radius

	std::vector<float> weights;
	weights.resize(2 * radius + 1);

	float sum = 0.0f;

	for (INT i = -radius; i <= +radius; ++i)
	{
		float x = i;

		weights[i + radius] = expf(-x*x / TwoSigmaSquare);

		sum += weights[i + radius];
	}

	for (UINT i = 0; i < weights.size(); ++i)
	{
		weights[i] /= sum;
	}

	return weights;
}

ID3D12Resource* Blur::output()
{
	return mBlurMap0.Get();
}