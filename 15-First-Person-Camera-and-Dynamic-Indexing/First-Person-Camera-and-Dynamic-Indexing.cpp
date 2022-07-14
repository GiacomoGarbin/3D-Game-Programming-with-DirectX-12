#include "ApplicationFramework.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"

#define RENDERDOC_BUILD 0

const int gFrameResourcesCount = 3;

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 world = MathHelper::Identity4x4();
	XMFLOAT4X4 TexCoordTransform = MathHelper::Identity4x4();

	int DirtyFramesCount = gFrameResourcesCount;

	UINT ConstantBufferIndex = -1;

	MeshGeometry* geometry = nullptr;
	Material* material = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	opaque = 0,
	count
};

class ApplicationInstance : public ApplicationFramework
{
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	UINT mCBVSRVUAVDescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCBVSRVUAVDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mMeshGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPipelineStateObjects;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::vector<std::unique_ptr<RenderItem>> mRenderItems;
	std::vector<RenderItem*> mLayerRenderItems[static_cast<int>(RenderLayer::count)];

	MainPassConstants mMainPassCB;
	//UINT mMainPassCBVOffset = 0;

	XMFLOAT3 mEyePosition = { 0.0f, 0.0f, 0.0f };

	bool mIsWireFrameEnabled = true;

	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mCameraTheta = 1.24f * XM_PI;
	float mCameraPhi = 0.42f * XM_PI;
	float mCameraRadius = 12.0f;

	POINT mLastMousePosition = { 0, 0 };

	virtual void CreateRTVAndDSVDescriptorHeaps() override;

	virtual void OnResize() override;
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;

	virtual void OnMouseDown(WPARAM state, int x, int y) override;
	virtual void OnMouseUp(WPARAM state, int x, int y) override;
	virtual void OnMouseMove(WPARAM state, int x, int y) override;
	void OnKeyboardEvent(const GameTimer& timer);

	void UpdateCamera(const GameTimer& timer);
	void AnimateMaterials(const GameTimer& timer);
	void UpdateObjectCBs(const GameTimer& timer);
	void UpdateMaterialCBs(const GameTimer& timer);
	void UpdateMainPassCB(const GameTimer& timer);

	void BuildRootSignatures();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildQuadPatchGeometry();
	void BuildPipelineStateObjects();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();

	void LoadTextures();
	const std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>& GetStaticSamplers();

	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList,
						 const std::vector<RenderItem*>& RenderItems);

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

	mCBVSRVUAVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildRootSignatures();
	BuildShadersAndInputLayout();
	BuildQuadPatchGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPipelineStateObjects();

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* CommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

	FlushCommandQueue();

	return true;
}

void ApplicationInstance::CreateRTVAndDSVDescriptorHeaps()
{
	// RTV
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = SwapChainBufferSize;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mRTVHeap.GetAddressOf())));
	}

	// DSV
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDSVHeap.GetAddressOf())));
	}

	// ImGUI SRV
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = 1;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mSRVHeap.GetAddressOf())));
	}
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

	AnimateMaterials(timer);
	UpdateObjectCBs(timer);
	UpdateMaterialCBs(timer);
	UpdateMainPassCB(timer);
}

void ApplicationInstance::draw(GameTimer& timer)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("settings");

		//ImGui::DragInt("blur count", &mBlur->GetCount(), 1, 0, 4);

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

	//mCommandList->SetPipelineState(mPipelineStateObjects["opaque"].Get());

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);
	}

	//{
	//	ID3D12DescriptorHeap* heaps[] = { mCBVSRVUAVDescriptorHeap.Get() };
	//	mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	//}

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto MainPassCB = mCurrentFrameResource->MainPassCB->GetResource();
	mCommandList->SetGraphicsRootConstantBufferView(3, MainPassCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::opaque)]);

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

		mCameraTheta += dx;
		mCameraPhi += dy;

		//mCameraPhi = MathHelper::clamp(mCameraPhi, 0.1f, XM_PI - 0.1f);
		mCameraPhi = clamp(mCameraPhi, 0.1f, XM_PI - 0.1f);
	}
	else if ((state & MK_RBUTTON) != 0)
	{
		float dx = 0.05f * static_cast<float>(x - mLastMousePosition.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePosition.y);

		mCameraRadius += dx - dy;

		//mCameraRadius = MathHelper::clamp(mCameraRadius, 5.0f, 150.0f);
		mCameraRadius = clamp(mCameraRadius, 5.0f, 150.0f);
	}

	mLastMousePosition.x = x;
	mLastMousePosition.y = y;
}

void ApplicationInstance::OnKeyboardEvent(const GameTimer& timer)
{
	//mIsWireFrameEnabled = (GetAsyncKeyState('1') & 0x8000);
}

void ApplicationInstance::UpdateCamera(const GameTimer& timer)
{
	mEyePosition.x = mCameraRadius * std::sin(mCameraPhi) * std::cos(mCameraTheta);
	mEyePosition.z = mCameraRadius * std::sin(mCameraPhi) * std::sin(mCameraTheta);
	mEyePosition.y = mCameraRadius * std::cos(mCameraPhi);

	XMVECTOR eye = XMVectorSet(mEyePosition.x, mEyePosition.y, mEyePosition.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
	XMStoreFloat4x4(&mView, view);
}

void ApplicationInstance::AnimateMaterials(const GameTimer& timer)
{}

void ApplicationInstance::UpdateObjectCBs(const GameTimer& timer)
{
	auto CurrentObjectCB = mCurrentFrameResource->ObjectCB.get();

	for (auto& object : mRenderItems)
	{
		if (object->DirtyFramesCount > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&object->world);
			XMMATRIX TexCoordTransform = XMLoadFloat4x4(&object->TexCoordTransform);

			ObjectConstants buffer;
			XMStoreFloat4x4(&buffer.world, XMMatrixTranspose(world));
			XMStoreFloat4x4(&buffer.TexCoordTransform, XMMatrixTranspose(TexCoordTransform));

			CurrentObjectCB->CopyData(object->ConstantBufferIndex, buffer);

			(object->DirtyFramesCount)--;
		}
	}
}

void ApplicationInstance::UpdateMaterialCBs(const GameTimer& timer)
{
	auto CurrentMaterialCB = mCurrentFrameResource->MaterialCB.get();

	for (auto& [key, material] : mMaterials)
	{
		if (material->DirtyFramesCount > 0)
		{
			XMMATRIX MaterialTransform = XMLoadFloat4x4(&material->transform);

			MaterialConstants buffer;
			buffer.DiffuseAlbedo = material->DiffuseAlbedo;
			buffer.FresnelR0 = material->FresnelR0;
			buffer.roughness = material->roughness;
			XMStoreFloat4x4(&buffer.transform, XMMatrixTranspose(MaterialTransform));

			CurrentMaterialCB->CopyData(material->ConstantBufferIndex, buffer);

			(material->DirtyFramesCount)--;
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
	mMainPassCB.DeltaTime = timer.GetDeltaTime();
	mMainPassCB.TotalTime = timer.GetTotalTime();

	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	mMainPassCB.lights[0].direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.lights[0].strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.lights[1].direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.lights[1].strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.lights[2].direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.lights[2].strength = { 0.15f, 0.15f, 0.15f };

	auto CurrentMainPassCB = mCurrentFrameResource->MainPassCB.get();
	CurrentMainPassCB->CopyData(0, mMainPassCB);
}

void ApplicationInstance::LoadTextures()
{
	//	// grass
	//	{
	//		auto texture = std::make_unique<Texture>();
	//		texture->name = "grass";
	//#if RENDERDOC_BUILD
	//		texture->filename = L"../../../../textures/grass.dds";
	//#else // RENDERDOC_BUILD
	//		texture->filename = L"../../textures/grass.dds";
	//#endif // RENDERDOC_BUILD
	//
	//		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
	//												 mCommandList.Get(),
	//												 texture->filename.c_str(),
	//												 texture->resource,
	//												 texture->UploadHeap));
	//
	//		mTextures[texture->name] = std::move(texture);
	//	}
	//
	//	// water1
	//	{
	//		auto texture = std::make_unique<Texture>();
	//		texture->name = "water1";
	//#if RENDERDOC_BUILD
	//		texture->filename = L"../../../../textures/water1.dds";
	//#else // RENDERDOC_BUILD
	//		texture->filename = L"../../textures/water1.dds";
	//#endif // RENDERDOC_BUILD
	//
	//		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
	//												 mCommandList.Get(),
	//												 texture->filename.c_str(),
	//												 texture->resource,
	//												 texture->UploadHeap));
	//
	//		mTextures[texture->name] = std::move(texture);
	//	}
	//
	//	// WoodCrate01
	//	{
	//		auto texture = std::make_unique<Texture>();
	//		texture->name = "WireFence";
	//#if RENDERDOC_BUILD
	//		texture->filename = L"../../../../textures/WireFence.dds";
	//#else // RENDERDOC_BUILD
	//		texture->filename = L"../../textures/WireFence.dds";
	//#endif // RENDERDOC_BUILD
	//
	//		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
	//												 mCommandList.Get(),
	//												 texture->filename.c_str(),
	//												 texture->resource,
	//												 texture->UploadHeap));
	//
	//		mTextures[texture->name] = std::move(texture);
	//	}
}

void ApplicationInstance::BuildRootSignatures()
{
	CD3DX12_DESCRIPTOR_RANGE TextureTable;
	TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER params[4];

	params[0].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	params[1].InitAsConstantBufferView(0);
	params[2].InitAsConstantBufferView(1);
	params[3].InitAsConstantBufferView(2);

	auto StaticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC desc(4,
									 params,
									 //StaticSamplers.size(),
									 //StaticSamplers.data(),
									 1,
									 &StaticSamplers[2],
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

void ApplicationInstance::BuildDescriptorHeaps()
{
	std::vector<std::pair<std::string, D3D12_SRV_DIMENSION>> textures =
	{
		//{ "grass", D3D12_SRV_DIMENSION_TEXTURE2D },
		//{ "water1", D3D12_SRV_DIMENSION_TEXTURE2D },
		//{ "WireFence", D3D12_SRV_DIMENSION_TEXTURE2D },
	};

	// create SRV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = textures.size();
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mCBVSRVUAVDescriptorHeap.GetAddressOf())));
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor(mCBVSRVUAVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto CreateSRV = [&](const std::string& name, D3D12_SRV_DIMENSION dimension)
	{
		const auto& texture = mTextures[name]->resource;

		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = texture->GetDesc().Format;
		desc.ViewDimension = dimension;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		switch (dimension)
		{
			case D3D12_SRV_DIMENSION_TEXTURE2D:
				desc.Texture2D.MostDetailedMip = 0;
				desc.Texture2D.ResourceMinLODClamp = 0.0f;
				desc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				desc.Texture2DArray.MostDetailedMip = 0;
				desc.Texture2DArray.MipLevels = texture->GetDesc().MipLevels;
				desc.Texture2DArray.FirstArraySlice = 0;
				desc.Texture2DArray.ArraySize = texture->GetDesc().DepthOrArraySize;
				break;
		}

		mDevice->CreateShaderResourceView(texture.Get(), &desc, descriptor);

		descriptor.Offset(1, mCBVSRVUAVDescriptorSize);
	};

	for (const auto& texture : textures)
	{
		CreateSRV(texture.first, texture.second);
	}
}

void ApplicationInstance::BuildShadersAndInputLayout()
{
#if RENDERDOC_BUILD
	mShaders["VS"] = Utils::CompileShader(L"../../shaders/tessellation.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["HS"] = Utils::CompileShader(L"../../shaders/tessellation.hlsl", nullptr, "HS", "hs_5_0");
	mShaders["DS"] = Utils::CompileShader(L"../../shaders/tessellation.hlsl", nullptr, "DS", "ds_5_0");
	mShaders["PS"] = Utils::CompileShader(L"../../shaders/tessellation.hlsl", nullptr, "PS", "ps_5_0");
#else // RENDERDOC_BUILD
	mShaders["VS"] = Utils::CompileShader(L"shaders/tessellation.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["HS"] = Utils::CompileShader(L"shaders/tessellation.hlsl", nullptr, "HS", "hs_5_0");
	mShaders["DS"] = Utils::CompileShader(L"shaders/tessellation.hlsl", nullptr, "DS", "ds_5_0");
	mShaders["PS"] = Utils::CompileShader(L"shaders/tessellation.hlsl", nullptr, "PS", "ps_5_0");
#endif // RENDERDOC_BUILD

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void ApplicationInstance::BuildQuadPatchGeometry()
{
	const std::array<XMFLOAT3, 16> vertices =
	{
		// row 0
		XMFLOAT3(-10.0f, -10.0f, +15.0f),
		XMFLOAT3(-05.0f, +00.0f, +15.0f),
		XMFLOAT3(+05.0f, +00.0f, +15.0f),
		XMFLOAT3(+10.0f, +00.0f, +15.0f),

		// row 1
		XMFLOAT3(-15.0f, +00.0f, +05.0f),
		XMFLOAT3(-05.0f, +00.0f, +05.0f),
		XMFLOAT3(+05.0f, +20.0f, +05.0f),
		XMFLOAT3(+15.0f, +00.0f, +05.0f),

		// row 2
		XMFLOAT3(-15.0f, +00.0f, -05.0f),
		XMFLOAT3(-05.0f, +00.0f, -05.0f),
		XMFLOAT3(+05.0f, +00.0f, -05.0f),
		XMFLOAT3(+15.0f, +00.0f, -05.0f),

		// row 3
		XMFLOAT3(-10.0f, +10.0f, -15.0f),
		XMFLOAT3(-05.0f, +00.0f, -15.0f),
		XMFLOAT3(+05.0f, +00.0f, -15.0f),
		XMFLOAT3(+25.0f, +10.0f, -15.0f)
	};

	const std::array<uint16_t, 16> indices =
	{
		0x0, 0x1, 0x2, 0x3,
		0x4, 0x5, 0x6, 0x7,
		0x8, 0x9, 0xa, 0xb,
		0xc, 0xd, 0xe, 0xf
	};

	const UINT VertexBufferByteSize = vertices.size() * sizeof(XMFLOAT3);
	const UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

	auto geometry = std::make_unique<MeshGeometry>();
	geometry->name = "QuadPatch";

	ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &geometry->VertexBufferCPU));
	CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), VertexBufferByteSize);

	ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &geometry->IndexBufferCPU));
	CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);

	geometry->VertexBufferGPU = Utils::CreateDefaultBuffer(mDevice.Get(),
														   mCommandList.Get(),
														   vertices.data(),
														   VertexBufferByteSize,
														   geometry->VertexBufferUploader);

	geometry->IndexBufferGPU = Utils::CreateDefaultBuffer(mDevice.Get(),
														  mCommandList.Get(),
														  indices.data(),
														  IndexBufferByteSize,
														  geometry->IndexBufferUploader);

	geometry->VertexByteStride = sizeof(XMFLOAT3);
	geometry->VertexBufferByteSize = VertexBufferByteSize;
	geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
	geometry->IndexBufferByteSize = IndexBufferByteSize;

	SubMeshGeometry SubMesh;
	SubMesh.IndexCount = indices.size();
	SubMesh.StartIndexLocation = 0;
	SubMesh.BaseVertexLocation = 0;

	geometry->DrawArgs["QuadPatch"] = SubMesh;

	mMeshGeometries[geometry->name] = std::move(geometry);
}

void ApplicationInstance::BuildPipelineStateObjects()
{
	std::map<std::string, D3D12_GRAPHICS_PIPELINE_STATE_DESC> descs;

	// opaque
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["opaque"];
		ZeroMemory(&desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

		desc.pRootSignature = mRootSignature.Get();
		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["VS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["VS"]->GetBufferSize();
		desc.HS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["HS"]->GetBufferPointer());
		desc.HS.BytecodeLength = mShaders["HS"]->GetBufferSize();
		desc.DS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["DS"]->GetBufferPointer());
		desc.DS.BytecodeLength = mShaders["DS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["PS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["PS"]->GetBufferSize();
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.SampleMask = UINT_MAX;
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		desc.InputLayout.pInputElementDescs = mInputLayout.data();
		desc.InputLayout.NumElements = mInputLayout.size();
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = mBackBufferFormat;
		desc.DSVFormat = mDepthStencilFormat;
		desc.SampleDesc.Count = m4xMSAAState ? 4 : 1;
		desc.SampleDesc.Quality = m4xMSAAState ? (m4xMSAAQuality - 1) : 0;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["opaque"])));
	}

	// opaque wireframe
	{
		descs["opaque_wireframe"] = descs["opaque"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["opaque_wireframe"];

		desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["opaque_wireframe"])));
	}
}

void ApplicationInstance::BuildFrameResources()
{
	for (int i = 0; i < gFrameResourcesCount; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(),
																  1,
																  mRenderItems.size(),
																  mMaterials.size()));
	}
}

void ApplicationInstance::BuildMaterials()
{
	UINT ConstantBufferIndex = 0;

	// white
	{
		auto material = std::make_unique<Material>();
		material->name = "white";
		material->ConstantBufferIndex = ConstantBufferIndex++;
		material->DiffuseSRVHeapIndex = material->ConstantBufferIndex; // 3
		material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
		material->roughness = 0.5f;

		mMaterials[material->name] = std::move(material);
	}
}

void ApplicationInstance::BuildRenderItems()
{
	UINT ObjectCBIndex = 0;

	// quad patch
	{
		auto item = std::make_unique<RenderItem>();

		item->world = MathHelper::Identity4x4();
		item->TexCoordTransform = MathHelper::Identity4x4();
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["QuadPatch"].get();
		item->material = mMaterials["white"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
		item->IndexCount = item->geometry->DrawArgs["QuadPatch"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["QuadPatch"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["QuadPatch"].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(RenderLayer::opaque)].push_back(item.get());

		mRenderItems.push_back(std::move(item));
	}
}

void ApplicationInstance::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems)
{
	const UINT ObjectCBByteSize = Utils::GetConstantBufferByteSize(sizeof(ObjectConstants));
	const auto ObjectCB = mCurrentFrameResource->ObjectCB->GetResource();

	const UINT MaterialCBByteSize = Utils::GetConstantBufferByteSize(sizeof(MaterialConstants));
	const auto MaterialCB = mCurrentFrameResource->MaterialCB->GetResource();

	for (const auto& item : RenderItems)
	{
		const D3D12_VERTEX_BUFFER_VIEW& VertexBufferView = item->geometry->GetVertexBufferView();
		CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
		const D3D12_INDEX_BUFFER_VIEW& IndexBufferView = item->geometry->GetIndexBufferView();
		CommandList->IASetIndexBuffer(&IndexBufferView);

		CommandList->IASetPrimitiveTopology(item->PrimitiveTopology);

		//CD3DX12_GPU_DESCRIPTOR_HANDLE srv(mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		//srv.Offset(item->material->DiffuseSRVHeapIndex, mCBVSRVUAVDescriptorSize);
		//CommandList->SetGraphicsRootDescriptorTable(0, srv);

		const D3D12_GPU_VIRTUAL_ADDRESS ObjectCBAddress = ObjectCB->GetGPUVirtualAddress() + item->ConstantBufferIndex * ObjectCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(1, ObjectCBAddress);

		const D3D12_GPU_VIRTUAL_ADDRESS MaterialCBAddress = MaterialCB->GetGPUVirtualAddress() + item->material->ConstantBufferIndex * MaterialCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(2, MaterialCBAddress); // 3

		CommandList->DrawIndexedInstanced(item->IndexCount, 1, item->StartIndexLocation, item->BaseVertexLocation, 0);
	}
}

const std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>& ApplicationInstance::GetStaticSamplers()
{
	static const std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> StaticSamplers = []()->std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>
	{
		const CD3DX12_STATIC_SAMPLER_DESC PointWrap
		(
			0,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		);

		const CD3DX12_STATIC_SAMPLER_DESC PointClamp
		(
			1,
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		);

		const CD3DX12_STATIC_SAMPLER_DESC LinearWrap
		(
			2,
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		);

		const CD3DX12_STATIC_SAMPLER_DESC LinearClamp
		(
			3,
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		const CD3DX12_STATIC_SAMPLER_DESC AnisotropicWrap
		(
			4,
			D3D12_FILTER_ANISOTROPIC,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			0.0f,
			8
		);

		const CD3DX12_STATIC_SAMPLER_DESC AnisotropicClamp
		(
			5,
			D3D12_FILTER_ANISOTROPIC,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			0.0f,
			8
		);

		return
		{
			PointWrap,
			PointClamp,
			LinearWrap,
			LinearClamp,
			AnisotropicWrap,
			AnisotropicClamp
		};
	}();

	return StaticSamplers;
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