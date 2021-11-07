#include "ApplicationFramework.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"

#include <fstream>
#include <map>

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
	mirror,
	reflex,
	transparent,
	shadow,
	
	count
};

class ApplicationInstance : public ApplicationFramework
{
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	UINT mCBVSRVDescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSRVDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mMeshGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPipelineStateObjects;

	RenderItem* mSkullRenderItem = nullptr;
	RenderItem* mReflexSkullRenderItem = nullptr;
	RenderItem* mShadowSkullRenderItem = nullptr;

	std::vector<std::unique_ptr<RenderItem>> mRenderItems;
	std::vector<RenderItem*> mLayerRenderItems[static_cast<int>(RenderLayer::count)];

	MainPassConstants mMainPassCB;
	//UINT mMainPassCBVOffset = 0;
	MainPassConstants mReflexPassCB;

	XMFLOAT3 mSkullTranslation = { 0.0f, 1.0f, -5.0f };

	XMFLOAT3 mEyePosition = { 0.0f, 0.0f, 0.0f };

	bool mIsWireFrameEnabled = false;

	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mCameraTheta = 1.24f * XM_PI;
	float mCameraPhi = 0.42f * XM_PI;
	float mCameraRadius = 12.0f;

	POINT mLastMousePosition;

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
	void UpdateReflexPassCB(const GameTimer& timer);

	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildRoomGeometry();
	void BuildSkullGeometry();
	void BuildPipelineStateObjects();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	
	void LoadTextures();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems);

	float GetHillHeight(float x, float z) const;
	XMFLOAT3 GetHillNormal(float x, float z) const;

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

	mCBVSRVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildRoomGeometry();
	BuildSkullGeometry();
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
	UpdateReflexPassCB(timer);
}

void ApplicationInstance::draw(GameTimer& timer)
{
	//ImGui_ImplDX12_NewFrame();
	//ImGui_ImplWin32_NewFrame();
	//ImGui::NewFrame();

	//{
	//	ImGui::Begin("clear render target color");
	//	ImGui::End();
	//}

	//ImGui::Render();

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

	mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);
	}

	{
		ID3D12DescriptorHeap* heaps[] = { mSRVDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	}

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	UINT MainPassCBByteSize = Utils::GetConstantBufferByteSize(sizeof(MainPassConstants));
	auto MainPassCB = mCurrentFrameResource->MainPassCB->GetResource();

	// draw opaque items : floor, wall & skull
	{
		mCommandList->SetGraphicsRootConstantBufferView(3, MainPassCB->GetGPUVirtualAddress());

		DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::opaque)]);
	}

	// mark the visible mirror pixels in the stencil buffer with the value 1
	{
		mCommandList->OMSetStencilRef(1);
		mCommandList->SetPipelineState(mPipelineStateObjects["mirror"].Get());

		DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::mirror)]);
	}

	// draw the reflected skull into the mirror only
	{
		// we supply a different per-pass CB with the lights reflected
		mCommandList->SetGraphicsRootConstantBufferView(3, MainPassCB->GetGPUVirtualAddress() + 1 * MainPassCBByteSize);
		mCommandList->SetPipelineState(mPipelineStateObjects["reflex"].Get());

		DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::reflex)]);
	}

	// draw mirror with transparency
	{
		// we restore main pass constants and stencil reference value
		mCommandList->OMSetStencilRef(0);
		mCommandList->SetGraphicsRootConstantBufferView(3, MainPassCB->GetGPUVirtualAddress());
		mCommandList->SetPipelineState(mPipelineStateObjects["transparent"].Get());

		DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::transparent)]);
	}

	// draw shadow
	{
		mCommandList->SetPipelineState(mPipelineStateObjects["shadow"].Get());

		DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::shadow)]);
	}

	//{
	//	ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get() };
	//	mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

	//	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	//}

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
	mIsWireFrameEnabled = (GetAsyncKeyState('1') & 0x8000);

	float dt = timer.GetDeltaTime();

	if (GetAsyncKeyState('A') & 0x8000)
	{
		mSkullTranslation.x -= dt;
	}

	if (GetAsyncKeyState('D') & 0x8000)
	{
		mSkullTranslation.x += dt;
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		mSkullTranslation.y -= dt;
	}

	if (GetAsyncKeyState('W') & 0x8000)
	{
		mSkullTranslation.y += dt;
	}

	auto max = [](const float& a, const float& b) -> float
	{
		return a < b ? a : b;
	};

	mSkullTranslation.y = max(mSkullTranslation.y, 0.0f);
	
	XMMATRIX SkullWorld;

	// update skull world matrix
	{
		XMMATRIX R = XMMatrixRotationY(0.5f * XM_PI * timer.GetTotalTime());
		XMMATRIX S = XMMatrixScaling(0.45f, 0.45f, 0.45f);
		XMMATRIX T = XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);
		SkullWorld = R * S * T;
		XMStoreFloat4x4(&mSkullRenderItem->world, SkullWorld);
	}

	// update reflected skull world matrix
	{
		XMVECTOR MirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
		XMMATRIX R = XMMatrixReflect(MirrorPlane);
		XMStoreFloat4x4(&mReflexSkullRenderItem->world, SkullWorld * R);
	}

	// update shadowed skull world matrix
	{
		XMVECTOR ShadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
		XMVECTOR ToMainLight = -XMLoadFloat3(&mMainPassCB.lights[0].direction);
		XMMATRIX S = XMMatrixShadow(ShadowPlane, ToMainLight);
		XMMATRIX bias = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&mShadowSkullRenderItem->world, SkullWorld * S * bias);
	}

	mSkullRenderItem->DirtyFramesCount = gFrameResourcesCount;
	mReflexSkullRenderItem->DirtyFramesCount = gFrameResourcesCount;
	mShadowSkullRenderItem->DirtyFramesCount = gFrameResourcesCount;
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
{

}

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

	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f};

	mMainPassCB.lights[0].direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.lights[0].strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.lights[1].direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.lights[1].strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.lights[2].direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.lights[2].strength = { 0.15f, 0.15f, 0.15f };

	auto CurrentMainPassCB = mCurrentFrameResource->MainPassCB.get();
	CurrentMainPassCB->CopyData(0, mMainPassCB);
}

void ApplicationInstance::UpdateReflexPassCB(const GameTimer& timer)
{
	mReflexPassCB = mMainPassCB;

	XMVECTOR MirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect(MirrorPlane);

	for (size_t i = 0; i < 3; ++i)
	{
		XMVECTOR direction = XMLoadFloat3(&mReflexPassCB.lights[i].direction);
		XMVECTOR reflected = XMVector3TransformNormal(direction, R);

		XMStoreFloat3(&mReflexPassCB.lights[i].direction, reflected);
	}

	auto CurrentReflexPassCB = mCurrentFrameResource->MainPassCB.get();
	CurrentReflexPassCB->CopyData(1, mReflexPassCB);
}

void ApplicationInstance::LoadTextures()
{
	auto LoadTexture = [&](const std::string& name)
	{
#if RENDERDOC_BUILD
		std::wstring filename = L"../../../textures/" + std::wstring(name.begin(), name.end()) + L".dds";
#else // RENDERDOC_BUILD
		std::wstring filename = L"../textures/" + std::wstring(name.begin(), name.end()) + L".dds";
#endif // RENDERDOC_BUILD

		auto texture = std::make_unique<Texture>();
		texture->name = name;
		texture->filename = filename;

		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
												 mCommandList.Get(),
												 texture->filename.c_str(),
												 texture->resource,
												 texture->UploadHeap));

		mTextures[texture->name] = std::move(texture);
	};

	std::vector<std::string> names =
	{
		"bricks3",
		"checkboard",
		"ice",
		"white1x1"
	};

	for (const std::string& name : names)
	{
		LoadTexture(name);
	}
}

void ApplicationInstance::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE TextureTable;
	TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER params[4];

	// performance TIP : order from most frequent to least frequent
	params[0].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	params[1].InitAsConstantBufferView(0);
	params[2].InitAsConstantBufferView(1);
	params[3].InitAsConstantBufferView(2);

	auto StaticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC desc(4,
									 params,
									 StaticSamplers.size(),
									 StaticSamplers.data(),
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
	// create SRV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 4;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mSRVDescriptorHeap.GetAddressOf())));
	}

	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor(mSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0;
		desc.Texture2D.ResourceMinLODClamp = 0.0f;

		std::vector<std::string> names =
		{
			"bricks3",
			"checkboard",
			"ice",
			"white1x1"
		};

		auto CreateSRV = [&](const auto& iter)
		{
			const auto& texture = mTextures[*iter]->resource;

			desc.Format = texture->GetDesc().Format;
			desc.Texture2D.MipLevels = texture->GetDesc().MipLevels;

			mDevice->CreateShaderResourceView(texture.Get(), &desc, descriptor);

			descriptor.Offset(1, mCBVSRVDescriptorSize);
		};

		for (auto iter = names.begin(); iter != names.end(); ++iter)
		{
			CreateSRV(iter);
		}
	}
}

void ApplicationInstance::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		nullptr, nullptr
	};
	
	const D3D_SHADER_MACRO AlphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		nullptr, nullptr
	};

#if RENDERDOC_BUILD
	mShaders["VS"] = Utils::CompileShader(L"../../shaders/default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["PS"] = Utils::CompileShader(L"../../shaders/default.hlsl", defines, "PS", "ps_5_0");
	mShaders["AlphaTestedPS"] = Utils::CompileShader(L"../../shaders/default.hlsl", AlphaTestDefines, "PS", "ps_5_0");
#else // RENDERDOC_BUILD
	mShaders["VS"] = Utils::CompileShader(L"shaders/default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["PS"] = Utils::CompileShader(L"shaders/default.hlsl", defines, "PS", "ps_5_0");
	mShaders["AlphaTestedPS"] = Utils::CompileShader(L"shaders/default.hlsl", AlphaTestDefines, "PS", "ps_5_0");
#endif // RENDERDOC_BUILD

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void ApplicationInstance::BuildRoomGeometry()
{
	std::array<Vertex, 20> vertices =
	{
		// floor
		Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex( 7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex( 7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

		// wall
		Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex( 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex( 2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex( 7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex( 7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex( 7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex( 7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// mirror
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex( 2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex( 2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	std::array<std::int16_t, 30> indices =
	{
		// floor
		0, 1, 2,
		0, 2, 3,

		// wall
		4, 5, 6,
		4, 6, 7,

		8,  9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// mirror
		16, 17, 18,
		16, 18, 19
	};

	UINT VertexBufferByteSize = vertices.size() * sizeof(Vertex);
	UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

	auto geometry = std::make_unique<MeshGeometry>();
	geometry->name = "room";

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

	// floor
	{
		SubMeshGeometry SubMesh;
		SubMesh.IndexCount = 6;
		SubMesh.StartIndexLocation = 0;
		SubMesh.BaseVertexLocation = 0;

		geometry->DrawArgs["floor"] = SubMesh;
	}

	// wall
	{
		SubMeshGeometry SubMesh;
		SubMesh.IndexCount = 18;
		SubMesh.StartIndexLocation = 6;
		SubMesh.BaseVertexLocation = 0;

		geometry->DrawArgs["wall"] = SubMesh;
	}
	
	// mirror
	{
		SubMeshGeometry SubMesh;
		SubMesh.IndexCount = 6;
		SubMesh.StartIndexLocation = 24;
		SubMesh.BaseVertexLocation = 0;

		geometry->DrawArgs["mirror"] = SubMesh;
	}

	mMeshGeometries[geometry->name] = std::move(geometry);
}

void ApplicationInstance::BuildSkullGeometry()
{
#if RENDERDOC_BUILD
	std::ifstream stream("../../../models/skull.txt");
#else // RENDERDOC_BUILD
	std::ifstream stream("../models/skull.txt");
#endif // RENDERDOC_BUILD

	if (!stream)
	{
		MessageBox(0, L"models/skull.txt not found.", 0, 0);
		return;
	}

	UINT VertexCount = 0;
	UINT IndexCount = 0;

	std::string ignore;

	stream >> ignore >> VertexCount;
	stream >> ignore >> IndexCount;
	stream >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(VertexCount);

	for (size_t i = 0; i < VertexCount; ++i)
	{
		stream >> vertices[i].position.x >> vertices[i].position.y >> vertices[i].position.z;
		stream >> vertices[i].normal.x >> vertices[i].normal.y >> vertices[i].normal.z;

		// model does not have texture coordinates, so just zero them out.
		vertices[i].TexCoord = { 0.0f, 0.0f };
	}

	stream >> ignore;
	stream >> ignore;
	stream >> ignore;

	std::vector<std::int32_t> indices(3 * IndexCount);

	for (size_t i = 0; i < IndexCount; ++i)
	{
		stream >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	stream.close();

	const UINT VertexBufferByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT IndexBufferByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geometry = std::make_unique<MeshGeometry>();
	geometry->name = "skull";

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

	geometry->VertexByteStride = sizeof(Vertex);
	geometry->VertexBufferByteSize = VertexBufferByteSize;
	geometry->IndexFormat = DXGI_FORMAT_R32_UINT;
	geometry->IndexBufferByteSize = IndexBufferByteSize;

	SubMeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geometry->DrawArgs["skull"] = submesh;

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
		descs["opaque_wireframe"] = descs["opaque"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["opaque_wireframe"];

		desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["opaque_wireframe"])));
	}

	// transparent
	{
		descs["transparent"] = descs["opaque"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["transparent"];

		desc.BlendState.RenderTarget[0].BlendEnable = true;
		desc.BlendState.RenderTarget[0].LogicOpEnable = false;
		desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["transparent"])));
	}

	// mirror
	{
		descs["mirror"] = descs["opaque"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["mirror"];

		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;

		desc.DepthStencilState.DepthEnable = true;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		
		desc.DepthStencilState.StencilEnable = true;
		desc.DepthStencilState.StencilReadMask = 0xff;
		desc.DepthStencilState.StencilWriteMask = 0xff;

		desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["mirror"])));
	}

	// reflex
	{
		descs["reflex"] = descs["opaque"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["reflex"];

		desc.DepthStencilState.DepthEnable = true;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		
		desc.DepthStencilState.StencilEnable = true;
		desc.DepthStencilState.StencilReadMask = 0xff;
		desc.DepthStencilState.StencilWriteMask = 0xff;

		desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

		desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

		desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.RasterizerState.FrontCounterClockwise = true;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["reflex"])));
	}

	// shadow
	{
		descs["shadow"] = descs["transparent"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["shadow"];

		desc.DepthStencilState.DepthEnable = true;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

		desc.DepthStencilState.StencilEnable = true;
		desc.DepthStencilState.StencilReadMask = 0xff;
		desc.DepthStencilState.StencilWriteMask = 0xff;

		desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
		desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

		desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
		desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["shadow"])));
	}
}

void ApplicationInstance::BuildFrameResources()
{
	for (int i = 0; i < gFrameResourcesCount; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(), 2, mRenderItems.size(), mMaterials.size()));
	}
}

void ApplicationInstance::BuildMaterials()
{
	UINT ConstantBufferIndex = 0;

	auto BuildMaterial = [&](const std::string& name,
							 const XMFLOAT4& DiffuseAlbedo,
							 const XMFLOAT3& FresnelR0,
							 float roughness)
	{
		auto material = std::make_unique<Material>();
		material->name = name;
		material->ConstantBufferIndex = ConstantBufferIndex++;
		material->DiffuseSRVHeapIndex = material->ConstantBufferIndex;
		material->DiffuseAlbedo = DiffuseAlbedo;
		material->FresnelR0 = FresnelR0;
		material->roughness = roughness;

		mMaterials[material->name] = std::move(material);
	};

	BuildMaterial("bricks",
				  XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
				  XMFLOAT3(0.05f, 0.05f, 0.05f),
				  0.25f);

	BuildMaterial("checkboard",
				  XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
				  XMFLOAT3(0.07f, 0.07f, 0.07f),
				  0.3f);

	BuildMaterial("ice",
				  XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f),
				  XMFLOAT3(0.1f, 0.1f, 0.1f),
				  0.5f);

	BuildMaterial("skull",
				  XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
				  XMFLOAT3(0.05f, 0.05f, 0.05f),
				  0.3f);

	//BuildMaterial("shadow",
	//			  XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f),
	//			  XMFLOAT3(0.001f, 0.001f, 0.001f),
	//			  0.0f);

	{
		auto material = std::make_unique<Material>();
		material->name = "shadow";
		material->ConstantBufferIndex = 4;
		material->DiffuseSRVHeapIndex = 3;
		material->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
		material->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
		material->roughness = 0.0f;

		mMaterials[material->name] = std::move(material);
	};
}

void ApplicationInstance::BuildRenderItems()
{
	UINT ObjectCBIndex = 0;

	// floor
	{
		auto item = std::make_unique<RenderItem>();

		item->world = MathHelper::Identity4x4();
		item->TexCoordTransform = MathHelper::Identity4x4();
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["room"].get();
		item->material = mMaterials["checkboard"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["floor"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["floor"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["floor"].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(RenderLayer::opaque)].push_back(item.get());
		mRenderItems.push_back(std::move(item));
	}

	// wall
	{
		auto item = std::make_unique<RenderItem>();

		item->world = MathHelper::Identity4x4();
		item->TexCoordTransform = MathHelper::Identity4x4();
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["room"].get();
		item->material = mMaterials["bricks"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["wall"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["wall"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["wall"].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(RenderLayer::opaque)].push_back(item.get());
		mRenderItems.push_back(std::move(item));
	}

	// skull
	{
		auto item = std::make_unique<RenderItem>();
		mSkullRenderItem = item.get();

		item->world = MathHelper::Identity4x4();
		item->TexCoordTransform = MathHelper::Identity4x4();
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["skull"].get();
		item->material = mMaterials["skull"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["skull"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["skull"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["skull"].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(RenderLayer::opaque)].push_back(item.get());
		mRenderItems.push_back(std::move(item));
	}

	// reflected skull
	{
		auto item = std::make_unique<RenderItem>();
		mReflexSkullRenderItem = item.get();

		*item = *mSkullRenderItem;
		item->ConstantBufferIndex = ObjectCBIndex++;

		mLayerRenderItems[static_cast<int>(RenderLayer::reflex)].push_back(item.get());
		mRenderItems.push_back(std::move(item));
	}

	// shadowed skull
	{
		auto item = std::make_unique<RenderItem>();
		mShadowSkullRenderItem = item.get();

		*item = *mSkullRenderItem;
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->material = mMaterials["shadow"].get();

		mLayerRenderItems[static_cast<int>(RenderLayer::shadow)].push_back(item.get());
		mRenderItems.push_back(std::move(item));
	}

	// mirror
	{
		auto item = std::make_unique<RenderItem>();

		item->world = MathHelper::Identity4x4();
		item->TexCoordTransform = MathHelper::Identity4x4();
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["room"].get();
		item->material = mMaterials["ice"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["mirror"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["mirror"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["mirror"].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(RenderLayer::mirror)].push_back(item.get());
		mLayerRenderItems[static_cast<int>(RenderLayer::transparent)].push_back(item.get());
		mRenderItems.push_back(std::move(item));
	}
}

void ApplicationInstance::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems)
{
	UINT ObjectCBByteSize = Utils::GetConstantBufferByteSize(sizeof(ObjectConstants));
	auto ObjectCB = mCurrentFrameResource->ObjectCB->GetResource();

	UINT MaterialCBByteSize = Utils::GetConstantBufferByteSize(sizeof(MaterialConstants));
	auto MaterialCB = mCurrentFrameResource->MaterialCB->GetResource();

	int i = 0;

	for (const auto& item : RenderItems)
	{
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView = item->geometry->GetVertexBufferView();
		CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
		D3D12_INDEX_BUFFER_VIEW IndexBufferView = item->geometry->GetIndexBufferView();
		CommandList->IASetIndexBuffer(&IndexBufferView);

		CommandList->IASetPrimitiveTopology(item->PrimitiveTopology);

		CD3DX12_GPU_DESCRIPTOR_HANDLE srv(mSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		srv.Offset(item->material->DiffuseSRVHeapIndex, mCBVSRVDescriptorSize);
		CommandList->SetGraphicsRootDescriptorTable(0, srv);

		D3D12_GPU_VIRTUAL_ADDRESS ObjectCBAddress = ObjectCB->GetGPUVirtualAddress();
		ObjectCBAddress += item->ConstantBufferIndex * ObjectCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(1, ObjectCBAddress);

		D3D12_GPU_VIRTUAL_ADDRESS MaterialCBAddress = MaterialCB->GetGPUVirtualAddress();
		MaterialCBAddress += item->material->ConstantBufferIndex * MaterialCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(2, MaterialCBAddress);

		CommandList->DrawIndexedInstanced(item->IndexCount, 1, item->StartIndexLocation, item->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ApplicationInstance::GetStaticSamplers()
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