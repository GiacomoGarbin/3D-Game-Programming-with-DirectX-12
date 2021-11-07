#include "FrameResource.h"

Vertex::Vertex()
{}

Vertex::Vertex(float px, float py, float pz,
			   float nx, float ny, float nz,
			   float tx, float ty) :
	position(px, py, pz),
	normal(nx, ny, nz),
	TexCoord(tx, ty)
{}

FrameResource::FrameResource(ID3D12Device* device,
							 UINT MainPassCount,
							 UINT ObjectCount,
							 UINT MaterialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(CommandAllocator.GetAddressOf())));

	MainPassCB = std::make_unique<UploadBuffer<MainPassConstants>>(device, MainPassCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, MaterialCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, ObjectCount, true);
}

FrameResource::~FrameResource()
{}