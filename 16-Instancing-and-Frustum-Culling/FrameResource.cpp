#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device,
							 const UINT MainPassCount,
							 const UINT MaxInstanceCount,
							 const UINT MaterialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
												 IID_PPV_ARGS(CommandAllocator.GetAddressOf())));

	MainPassCB = std::make_unique<UploadBuffer<MainPassConstants>>(device, MainPassCount, true);
	InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, MaxInstanceCount, false);
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, MaterialCount, false);
}

FrameResource::~FrameResource()
{}