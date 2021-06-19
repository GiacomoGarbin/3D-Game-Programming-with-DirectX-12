#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT MainPassCount, UINT MaterialCount, UINT ObjectCount, UINT WaveVertexCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(CommandAllocator.GetAddressOf())));

	MainPassCB = std::make_unique<UploadBuffer<MainPassConstants>>(device, MainPassCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, MaterialCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, ObjectCount, true);

	WavesVB = std::make_unique<UploadBuffer<Vertex>>(device, WaveVertexCount, false);
}

FrameResource::~FrameResource()
{}