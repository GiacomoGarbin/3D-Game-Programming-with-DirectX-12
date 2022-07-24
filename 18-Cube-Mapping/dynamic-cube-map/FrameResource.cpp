#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device,
							 const UINT MainPassCount,
							 const UINT ObjectCount,
							 const UINT MaterialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
												 IID_PPV_ARGS(CommandAllocator.GetAddressOf())));

	MainPassCB = std::make_unique<UploadBuffer<MainPassConstants>>(device, MainPassCount, true);
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, MaterialCount, false);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, ObjectCount, true);
}

FrameResource::~FrameResource()
{}