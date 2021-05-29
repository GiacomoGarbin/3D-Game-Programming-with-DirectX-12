#include "ApplicationFramework.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"

const int gFrameResourcesCount = 3;

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 world = MathHelper::Identity4x4();

	int DirtyFrames = gFrameResourcesCount;

	UINT ConstantBufferIndex = -1;

	MeshGeometry* geometry = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class ApplicationInstance : public ApplicationFramework
{
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	XMFLOAT4 mRenderTargetClearColor = { 1, 1, 0, 1 };
	//XMVECTORF32 mRenderTargetClearColor = DirectX::Colors::Yellow;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCBVHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mMeshGeometries;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPipelineStateObjects;

	std::vector<std::unique_ptr<RenderItem>> mRenderItems;
	std::vector<RenderItem*> mOpaqueRenderItems;

	MainPassConstants mMainPassCB;
	UINT mMainPassCBVOffset = 0;

	XMFLOAT3 mEyePosition = { 0.0f, 0.0f, 0.0f };

	bool mIsWireFrameEnabled = false;
	
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePosition;

	virtual void OnResize() override;
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;

	virtual void OnMouseDown(WPARAM state, int x, int y) override;
	virtual void OnMouseUp(WPARAM state, int x, int y) override;
	virtual void OnMouseMove(WPARAM state, int x, int y) override;
	void OnKeyboardEvent(const GameTimer& timer);

	void UpdateCamera(const GameTimer& timer);

	void UpdateObjectCBs(const GameTimer& timer);
	void UpdateMainPassCB(const GameTimer& timer);

	void BuildDescriptorHeap();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildMeshGeometry();
	void BuildPipelineStateObjects();
	void BuildFrameResources();
	void BuildRenderItems();
	
	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems);

public:
	ApplicationInstance(HINSTANCE instance);
	~ApplicationInstance();

	virtual bool init() override;
};

ApplicationInstance::ApplicationInstance(HINSTANCE instance)
	: ApplicationFramework(instance)
{}

ApplicationInstance::~ApplicationInstance()
{
	if (mDevice != nullptr)
	{
		FlushCommandQueue();
	}
}

bool ApplicationInstance::init()
{
	if (!ApplicationFramework::init())
	{
		return false;
	}

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildMeshGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeap();
	BuildConstantBufferViews();
	BuildPipelineStateObjects();

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* CommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

	FlushCommandQueue();

	return true;
}

void ApplicationInstance::OnResize()
{
	ApplicationFramework::OnResize();

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, GetAspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void ApplicationInstance::update(GameTimer& timer)
{
	OnKeyboardEvent(timer);
	UpdateCamera(timer);

	mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % gFrameResourcesCount;
	mCurrentFrameResource = mFrameResources[mCurrentFrameResourceIndex].get();

	if (mCurrentFrameResource->fence != 0 && mFence->GetCompletedValue() < mCurrentFrameResource->fence)
	{
		HANDLE handle = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameResource->fence, handle));
		WaitForSingleObject(handle, INFINITE);
		CloseHandle(handle);
	}

	UpdateObjectCBs(timer);
	UpdateMainPassCB(timer);
}

void ApplicationInstance::draw(GameTimer& timer)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("clear render target color");
		ImGui::ColorEdit3("clear render target color", &mRenderTargetClearColor.x, 0);
		ImGui::End();
	}

	ImGui::Render();

	auto CommandAllocator = mCurrentFrameResource->CommandAllocator;

	ThrowIfFailed(CommandAllocator->Reset());

	if (mIsWireFrameEnabled)
	{
		ThrowIfFailed(mCommandList->Reset(CommandAllocator.Get(), mPipelineStateObjects["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(CommandAllocator.Get(), mPipelineStateObjects["opaque"].Get()));
	}
	
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), &mRenderTargetClearColor.x, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);
	}

	ID3D12DescriptorHeap* heaps[] = { mCBVHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int MainPassCBVIndex = mMainPassCBVOffset + mCurrentFrameResourceIndex;
	auto MainPassCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVHeap->GetGPUDescriptorHandleForHeapStart());
	MainPassCBVHandle.Offset(MainPassCBVIndex, mCBVSRVUAVDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, MainPassCBVHandle);

	DrawRenderItems(mCommandList.Get(), mOpaqueRenderItems);

	{
		ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	}

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCommandList->ResourceBarrier(1, &transition);
	}

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* CommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrentBackBufferIndex = (mCurrentBackBufferIndex + 1) % SwapChainBufferSize;

	mCurrentFrameResource->fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ApplicationInstance::OnMouseDown(WPARAM state, int x, int y)
{
	mLastMousePosition.x = x;
	mLastMousePosition.y = y;

	SetCapture(mMainWindow);
}

void ApplicationInstance::OnMouseUp(WPARAM state, int x, int y)
{
	ReleaseCapture();
}

void ApplicationInstance::OnMouseMove(WPARAM state, int x, int y)
{
	auto clamp = [](const float& x, const float& a, const float& b) -> float
	{
		return x < a ? a : (x > b ? b : x);
	};

	if ((state & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePosition.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePosition.y));

		mTheta += dx;
		mPhi += dy;

		//mPhi = MathHelper::clamp(mPhi, 0.1f, XM_PI - 0.1f);
		mPhi = clamp(mPhi, 0.1f, XM_PI - 0.1f);
	}
	else if ((state & MK_RBUTTON) != 0)
	{
		float dx = 0.05f * static_cast<float>(x - mLastMousePosition.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePosition.y);

		mRadius += dx - dy;

		//mRadius = MathHelper::clamp(mRadius, 5.0f, 150.0f);
		mRadius = clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePosition.x = x;
	mLastMousePosition.y = y;
}

void ApplicationInstance::OnKeyboardEvent(const GameTimer& timer)
{
	mIsWireFrameEnabled = (GetAsyncKeyState('1') & 0x8000);
}

void ApplicationInstance::UpdateCamera(const GameTimer& timer)
{
	mEyePosition.x = mRadius * std::sin(mPhi) * std::cos(mTheta);
	mEyePosition.z = mRadius * std::sin(mPhi) * std::sin(mTheta);
	mEyePosition.y = mRadius * std::cos(mPhi);

	XMVECTOR eye = XMVectorSet(mEyePosition.x, mEyePosition.y, mEyePosition.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
	XMStoreFloat4x4(&mView, view);
}

void ApplicationInstance::UpdateObjectCBs(const GameTimer& timer)
{
	auto CurrentObjectCB = mCurrentFrameResource->ObjectCB.get();

	for (auto& object : mRenderItems)
	{
		if (object->DirtyFrames > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&object->world);

			ObjectConstants buffer;
			XMStoreFloat4x4(&buffer.world, XMMatrixTranspose(world));

			CurrentObjectCB->CopyData(object->ConstantBufferIndex, buffer);

			(object->DirtyFrames)--;
		}
	}
}

void ApplicationInstance::UpdateMainPassCB(const GameTimer& timer)
{
	XMVECTOR determinant;

	XMMATRIX view = XMLoadFloat4x4(&mView);
	determinant = XMMatrixDeterminant(view);
	XMMATRIX ViewInverse = XMMatrixInverse(&determinant, view);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	determinant = XMMatrixDeterminant(proj);
	XMMATRIX ProjInverse = XMMatrixInverse(&determinant, proj);
	XMMATRIX ViewProj = view * proj;
	determinant = XMMatrixDeterminant(ViewProj);
	XMMATRIX ViewProjInverse = XMMatrixInverse(&determinant, ViewProj);

	XMStoreFloat4x4(&mMainPassCB.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.ViewInverse, XMMatrixTranspose(ViewInverse));
	XMStoreFloat4x4(&mMainPassCB.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.ProjInverse, XMMatrixTranspose(ProjInverse));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjInverse, XMMatrixTranspose(ViewProjInverse));
	mMainPassCB.EyePositionWorld = mEyePosition;
	mMainPassCB.RenderTargetSize = XMFLOAT2(mMainWindowWidth, mMainWindowHeight);
	mMainPassCB.RenderTargetSizeInverse = XMFLOAT2(1.0f / mMainWindowWidth, 1.0f / mMainWindowHeight);
	mMainPassCB.NearPlane = 1.0f;
	mMainPassCB.FarPlane = 1000.0f;
	mMainPassCB.DeltaTime = mGameTimer.GetDeltaTime();
	mMainPassCB.TotalTime = mGameTimer.GetTotalTime();

	auto CurrentMainPassCB = mCurrentFrameResource->MainPassCB.get();
	CurrentMainPassCB->CopyData(0, mMainPassCB);
}

void ApplicationInstance::BuildDescriptorHeap()
{
	UINT count = mOpaqueRenderItems.size();
	mMainPassCBVOffset = count * gFrameResourcesCount;

	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.NumDescriptors = (count + 1) * gFrameResourcesCount;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mCBVHeap.GetAddressOf())));
}

void ApplicationInstance::BuildConstantBufferViews()
{
	UINT count = mOpaqueRenderItems.size();

	UINT ObjectCBByteSize = Utils::GetConstantBufferByteSize(sizeof(ObjectConstants));
	UINT MainPassCBByteSize = Utils::GetConstantBufferByteSize(sizeof(MainPassConstants));

	for (int i = 0; i < gFrameResourcesCount; ++i)
	{
		auto ObjectCB = mFrameResources[i]->ObjectCB->GetResource();

		for (int j = 0; j < count; ++j)
		{
			D3D12_GPU_VIRTUAL_ADDRESS address = ObjectCB->GetGPUVirtualAddress();
			address += j * ObjectCBByteSize;

			int index = i * count + j;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(index, mCBVSRVUAVDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			desc.BufferLocation = address;
			desc.SizeInBytes = ObjectCBByteSize;

			mDevice->CreateConstantBufferView(&desc, handle);
		}

		auto MainPassCB = mFrameResources[i]->MainPassCB->GetResource();

		{
			D3D12_GPU_VIRTUAL_ADDRESS address = MainPassCB->GetGPUVirtualAddress();

			int index = mMainPassCBVOffset + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(index, mCBVSRVUAVDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
			desc.BufferLocation = address;
			desc.SizeInBytes = MainPassCBByteSize;

			mDevice->CreateConstantBufferView(&desc, handle);
		}
	}
}

void ApplicationInstance::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER params[2];

	// CBV table 0
	{
		CD3DX12_DESCRIPTOR_RANGE table;
		table.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
		params[0].InitAsDescriptorTable(1, &table);
	}

	// CBV table 1
	{
		CD3DX12_DESCRIPTOR_RANGE table;
		table.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
		params[1].InitAsDescriptorTable(1, &table);
	}

	CD3DX12_ROOT_SIGNATURE_DESC desc(2,
									 params,
									 0,
									 nullptr,
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

void ApplicationInstance::BuildShadersAndInputLayout()
{
	mShaders["VS"] = Utils::CompileShader(L"shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["PS"] = Utils::CompileShader(L"shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void ApplicationInstance::BuildMeshGeometry()
{
	GeometryGenerator generator;

	GeometryGenerator::MeshData box = generator.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = generator.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData cylinder = generator.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData sphere = generator.CreateSphere(0.5f, 20, 20);

	UINT BoxVertexOffset = 0;
	UINT GridVertexOffset = box.vertices.size();
	UINT CylinderVertexOffset = GridVertexOffset + grid.vertices.size();
	UINT SphereVertexOffset = CylinderVertexOffset + cylinder.vertices.size();
	
	UINT BoxIndexOffset = 0;
	UINT GridIndexOffset = box.indices32.size();
	UINT CylinderIndexOffset = GridIndexOffset + grid.indices32.size();
	UINT SphereIndexOffset = CylinderIndexOffset + cylinder.indices32.size();

	SubMeshGeometry BoxSubMesh;
	BoxSubMesh.IndexCount = box.indices32.size();
	BoxSubMesh.StartIndexLocation = BoxIndexOffset;
	BoxSubMesh.BaseVertexLocation = BoxVertexOffset;

	SubMeshGeometry GridSubMesh;
	GridSubMesh.IndexCount = grid.indices32.size();
	GridSubMesh.StartIndexLocation = GridIndexOffset;
	GridSubMesh.BaseVertexLocation = GridVertexOffset;

	SubMeshGeometry CylinderSubMesh;
	CylinderSubMesh.IndexCount = cylinder.indices32.size();
	CylinderSubMesh.StartIndexLocation = CylinderIndexOffset;
	CylinderSubMesh.BaseVertexLocation = CylinderVertexOffset;

	SubMeshGeometry SphereSubMesh;
	SphereSubMesh.IndexCount = sphere.indices32.size();
	SphereSubMesh.StartIndexLocation = SphereIndexOffset;
	SphereSubMesh.BaseVertexLocation = SphereVertexOffset;

	size_t VertexCount = box.vertices.size() + grid.vertices.size() + cylinder.vertices.size() + sphere.vertices.size();
	std::vector<Vertex> vertices(VertexCount);
	auto i = vertices.begin();

	for (auto vertex : box.vertices)
	{
		i->position = vertex.position;
		i->color = XMFLOAT4(DirectX::Colors::DarkGreen);
		i++;
	}

	for (auto vertex : grid.vertices)
	{
		i->position = vertex.position;
		i->color = XMFLOAT4(DirectX::Colors::ForestGreen);
		i++;
	}

	for (auto vertex : cylinder.vertices)
	{
		i->position = vertex.position;
		i->color = XMFLOAT4(DirectX::Colors::Crimson);
		i++;
	}

	for (auto vertex : sphere.vertices)
	{
		i->position = vertex.position;
		i->color = XMFLOAT4(DirectX::Colors::SteelBlue);
		i++;
	}

	std::vector<uint16_t> indices;

	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());

	UINT VertexBufferByteSize = vertices.size() * sizeof(Vertex);
	UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

	auto geometry = std::make_unique<MeshGeometry>();
	geometry->name = "geometry";

	ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &geometry->VertexBufferCPU));
	CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), VertexBufferByteSize);

	ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &geometry->IndexBufferCPU));
	CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);

	geometry->VertexBufferGPU = Utils::CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), vertices.data(), VertexBufferByteSize, geometry->VertexBufferUploader);

	geometry->IndexBufferGPU = Utils::CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), indices.data(), IndexBufferByteSize, geometry->IndexBufferUploader);

	geometry->VertexByteStride = sizeof(Vertex);
	geometry->VertexBufferByteSize = VertexBufferByteSize;
	geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
	geometry->IndexBufferByteSize = IndexBufferByteSize;

	geometry->DrawArgs["box"] = BoxSubMesh;
	geometry->DrawArgs["grid"] = GridSubMesh;
	geometry->DrawArgs["cylinder"] = CylinderSubMesh;
	geometry->DrawArgs["sphere"] = SphereSubMesh;

	mMeshGeometries[geometry->name] = std::move(geometry);
}

void ApplicationInstance::BuildPipelineStateObjects()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	// opaque
	{
		desc.pRootSignature = mRootSignature.Get();
		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["VS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["VS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["PS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["PS"]->GetBufferSize();
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.SampleMask = UINT_MAX;
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		desc.InputLayout.pInputElementDescs = mInputLayout.data();
		desc.InputLayout.NumElements = mInputLayout.size();
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = mBackBufferFormat;
		desc.DSVFormat = mDepthStencilFormat;
		desc.SampleDesc.Count = m4xMSAAState ? 4 : 1;
		desc.SampleDesc.Quality = m4xMSAAState ? (m4xMSAAQuality - 1) : 0;
		
		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["opaque"])));
	}

	// opaque wireframe
	{
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["opaque_wireframe"])));
	}
}

void ApplicationInstance::BuildFrameResources()
{
	for (int i = 0; i < gFrameResourcesCount; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(), 1, mRenderItems.size()));
	}
}

void ApplicationInstance::BuildRenderItems()
{
	UINT ObjectCBIndex = 0;

	// box
	{
		auto item = std::make_unique<RenderItem>();

		XMStoreFloat4x4(&item->world, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["geometry"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["box"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["box"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["box"].BaseVertexLocation;

		mRenderItems.push_back(std::move(item));
	}

	// grid
	{
		auto item = std::make_unique<RenderItem>();

		item->world = MathHelper::Identity4x4();
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["geometry"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["grid"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["grid"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["grid"].BaseVertexLocation;

		mRenderItems.push_back(std::move(item));
	}

	for (int i = 0; i < 5; ++i)
	{
		// left cylinder
		{
			auto item = std::make_unique<RenderItem>();

			XMStoreFloat4x4(&item->world, XMMatrixTranslation(-5.0f, 1.5f, i * 5.0f - 10.0f));
			item->ConstantBufferIndex = ObjectCBIndex++;
			item->geometry = mMeshGeometries["geometry"].get();
			item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			item->IndexCount = item->geometry->DrawArgs["cylinder"].IndexCount;
			item->StartIndexLocation = item->geometry->DrawArgs["cylinder"].StartIndexLocation;
			item->BaseVertexLocation = item->geometry->DrawArgs["cylinder"].BaseVertexLocation;

			mRenderItems.push_back(std::move(item));
		}

		// right cylinder
		{
			auto item = std::make_unique<RenderItem>();

			XMStoreFloat4x4(&item->world, XMMatrixTranslation(+5.0f, 1.5f, i * 5.0f - 10.0f));
			item->ConstantBufferIndex = ObjectCBIndex++;
			item->geometry = mMeshGeometries["geometry"].get();
			item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			item->IndexCount = item->geometry->DrawArgs["cylinder"].IndexCount;
			item->StartIndexLocation = item->geometry->DrawArgs["cylinder"].StartIndexLocation;
			item->BaseVertexLocation = item->geometry->DrawArgs["cylinder"].BaseVertexLocation;

			mRenderItems.push_back(std::move(item));
		}

		// left sphere
		{
			auto item = std::make_unique<RenderItem>();

			XMStoreFloat4x4(&item->world, XMMatrixTranslation(-5.0f, 3.5f, i * 5.0f - 10.0f));
			item->ConstantBufferIndex = ObjectCBIndex++;
			item->geometry = mMeshGeometries["geometry"].get();
			item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			item->IndexCount = item->geometry->DrawArgs["sphere"].IndexCount;
			item->StartIndexLocation = item->geometry->DrawArgs["sphere"].StartIndexLocation;
			item->BaseVertexLocation = item->geometry->DrawArgs["sphere"].BaseVertexLocation;

			mRenderItems.push_back(std::move(item));
		}

		// right sphere
		{
			auto item = std::make_unique<RenderItem>();

			XMStoreFloat4x4(&item->world, XMMatrixTranslation(+5.0f, 3.5f, i * 5.0f - 10.0f));
			item->ConstantBufferIndex = ObjectCBIndex++;
			item->geometry = mMeshGeometries["geometry"].get();
			item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			item->IndexCount = item->geometry->DrawArgs["sphere"].IndexCount;
			item->StartIndexLocation = item->geometry->DrawArgs["sphere"].StartIndexLocation;
			item->BaseVertexLocation = item->geometry->DrawArgs["sphere"].BaseVertexLocation;

			mRenderItems.push_back(std::move(item));
		}
	}

	for (const auto& item : mRenderItems)
	{
		mOpaqueRenderItems.push_back(item.get());
	}
}

void ApplicationInstance::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems)
{
	for (const auto& item : RenderItems)
	{
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView = item->geometry->GetVertexBufferView();
		CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
		D3D12_INDEX_BUFFER_VIEW IndexBufferView = item->geometry->GetIndexBufferView();
		CommandList->IASetIndexBuffer(&IndexBufferView);
		CommandList->IASetPrimitiveTopology(item->PrimitiveTopology);

		int index = mCurrentFrameResourceIndex * mOpaqueRenderItems.size() + item->ConstantBufferIndex;
		auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVHeap->GetGPUDescriptorHandleForHeapStart());
		handle.Offset(index, mCBVSRVUAVDescriptorSize);
		CommandList->SetGraphicsRootDescriptorTable(0, handle);

		CommandList->DrawIndexedInstanced(item->IndexCount, 1, item->StartIndexLocation, item->BaseVertexLocation, 0);
	}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, PSTR CMD, int ShowCMD)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	try
	{
		ApplicationInstance app(instance);

		if (!app.init())
		{
			return 0;
		}

		return app.run();
	}
	catch (ApplicationFrameworkException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}