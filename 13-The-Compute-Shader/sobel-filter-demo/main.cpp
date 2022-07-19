#include "ApplicationFramework.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "waves.h"
#include "blur.h"
#include "sobel.h"
#include "RenderTarget.h"

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
	transparent,
	AlphaTested,

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

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPipelineStateObjects;

	std::vector<std::unique_ptr<RenderItem>> mRenderItems;
	std::vector<RenderItem*> mLayerRenderItems[static_cast<int>(RenderLayer::count)];

	RenderItem* mWavesRenderItem = nullptr;
	std::unique_ptr<Waves> mWaves;

	std::unique_ptr<Blur> mBlur;

	std::unique_ptr<RenderTarget> mOffscreenRT;
	std::unique_ptr<Sobel> mSobel;

	MainPassConstants mMainPassCB;
	UINT mMainPassCBVOffset = 0;

	XMFLOAT3 mEyePosition = { 0.0f, 0.0f, 0.0f };

	bool mIsWireFrameEnabled = false;

	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mCameraTheta = 1.5f * XM_PI;
	float mCameraPhi = XM_PIDIV2 - 0.1f;
	float mCameraRadius = 50.0f;

	POINT mLastMousePosition;

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
	void UpdateWaves(const GameTimer& timer);

	void BuildDescriptorHeaps();
	void BuildRootSignatures();
	void BuildShadersAndInputLayout();
	void BuildMeshGeometry(); // land, waves and box geometry
	void BuildPipelineStateObjects();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();

	void LoadTextures();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>& GetStaticSamplers();

	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems);
	void DrawFullscreenQuad(ID3D12GraphicsCommandList* CommandList);

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

	mCBVSRVUAVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//mWaves = std::make_unique<Waves>(256, 256, 0.03f, 0.25f, 2.0f, 0.2f);
	//mWaves = std::make_unique<Waves>(128, 128, 0.03f, 0.25f, 2.0f, 0.2f);
	mWaves = std::make_unique<Waves>(128, 128, 0.03f, 1.0f, 4.0f, 0.2f);

	mBlur = std::make_unique<Blur>(mDevice.Get(), mMainWindowWidth, mMainWindowHeight, mBackBufferFormat);

	mOffscreenRT = std::make_unique<RenderTarget>(mDevice.Get(), mMainWindowWidth, mMainWindowHeight, mBackBufferFormat);
	mSobel = std::make_unique<Sobel>(mDevice.Get(), mMainWindowWidth, mMainWindowHeight, mBackBufferFormat);

	LoadTextures();
	BuildRootSignatures();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildMeshGeometry();
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
		desc.NumDescriptors = SwapChainBufferSize + mOffscreenRT->DescriptorCount(); // +1 for offscreen render target
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

	if (mBlur)
	{
		mBlur->OnResize(mMainWindowWidth, mMainWindowHeight);
	}

	if (mSobel)
	{
		mSobel->OnResize(mMainWindowWidth, mMainWindowHeight);
	}

	if (mOffscreenRT)
	{
		mOffscreenRT->OnResize(mMainWindowWidth, mMainWindowHeight);
	}
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
	UpdateWaves(timer);
}

void ApplicationInstance::draw(GameTimer& timer)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("settings");

		ImGui::DragInt("blur count", &mBlur->GetCount(), 1, 0, 4);

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

	{
		ID3D12DescriptorHeap* heaps[] = { mCBVSRVUAVDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	}

	mCommandList->SetPipelineState(mPipelineStateObjects["opaque"].Get());

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mOffscreenRT->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	mCommandList->ClearRenderTargetView(mOffscreenRT->GetRTV(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = mOffscreenRT->GetRTV();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);
	}

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto MainPassCB = mCurrentFrameResource->MainPassCB->GetResource();
	mCommandList->SetGraphicsRootConstantBufferView(3, MainPassCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::opaque)]);

	mCommandList->SetPipelineState(mPipelineStateObjects["alpha_tested"].Get());
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::AlphaTested)]);

	mCommandList->SetPipelineState(mPipelineStateObjects["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::transparent)]);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mOffscreenRT->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
		mCommandList->ResourceBarrier(1, &transition);
	}

	mSobel->execute(mCommandList.Get(),
					mPipelineStateObjects["sobel"].Get(),
					mOffscreenRT->GetSRV());

	mBlur->execute(mCommandList.Get(),
				   mPipelineStateObjects["horz_blur"].Get(),
				   mPipelineStateObjects["vert_blur"].Get(),
				   mOffscreenRT->GetResource());

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mOffscreenRT->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		mCommandList->ResourceBarrier(1, &transition);
	}

	// copy blur output to back buffer
	mCommandList->CopyResource(mOffscreenRT->GetResource(), mBlur->output());

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);
	}

	mCommandList->SetGraphicsRootSignature(mSobel->GetRootSignature());
	mCommandList->SetPipelineState(mPipelineStateObjects["composite"].Get());
	mCommandList->SetGraphicsRootDescriptorTable(0, mOffscreenRT->GetSRV());
	mCommandList->SetGraphicsRootDescriptorTable(1, mSobel->output());
	DrawFullscreenQuad(mCommandList.Get());

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
	mIsWireFrameEnabled = (GetAsyncKeyState('1') & 0x8000);

	//float dt = timer.GetDeltaTime();

	//if (GetAsyncKeyState(VK_LEFT) & 0x8000)
	//{
	//	mLightTheta -= dt;
	//}

	//if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
	//{
	//	mLightTheta += dt;
	//}

	//if (GetAsyncKeyState(VK_UP) & 0x8000)
	//{
	//	mLightPhi -= dt;
	//}

	//if (GetAsyncKeyState(VK_DOWN) & 0x8000)
	//{
	//	mLightPhi += dt;
	//}
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
	auto material = mMaterials["water"].get();

	float& u = material->transform(3, 0);
	float& v = material->transform(3, 1);

	u += 0.10f * timer.GetDeltaTime();
	v += 0.02f * timer.GetDeltaTime();

	if (u >= 1.0f)
	{
		u -= 1.0f;
	}

	if (v >= 1.0f)
	{
		v -= 1.0f;
	}

	// do we need to assign u and v to material ?!

	material->DirtyFramesCount = gFrameResourcesCount;
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

void ApplicationInstance::UpdateWaves(const GameTimer& timer)
{
	static float BaseTime = 0.0f;

	if ((timer.GetTotalTime() - BaseTime) >= 0.25f)
	{
		BaseTime += 0.25f;

		int i = MathHelper::RandInt(4, mWaves->RowCount() - 5);
		int j = MathHelper::RandInt(4, mWaves->ColCount() - 5);

		float r = MathHelper::RandFloat(0.2f, 0.5f);

		mWaves->disturb(i, j, r);
	}

	mWaves->update(timer.GetDeltaTime());

	auto WavesVB = mCurrentFrameResource->WavesVB.get();

	for (int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex vertex;

		vertex.position = mWaves->GetPosition(i);
		vertex.normal = mWaves->GetNormal(i);

		vertex.TexCoord.x = 0.5f + vertex.position.x / mWaves->GetWidth();
		vertex.TexCoord.y = 0.5f - vertex.position.z / mWaves->GetDepth();

		WavesVB->CopyData(i, vertex);
	}

	mWavesRenderItem->geometry->VertexBufferGPU = WavesVB->GetResource();
}

void ApplicationInstance::LoadTextures()
{
	// grass
	{
		auto texture = std::make_unique<Texture>();
		texture->name = "grass";
#if RENDERDOC_BUILD
		texture->filename = L"../../../../textures/grass.dds";
#else // RENDERDOC_BUILD
		texture->filename = L"../../textures/grass.dds";
#endif // RENDERDOC_BUILD

		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
												 mCommandList.Get(),
												 texture->filename.c_str(),
												 texture->resource,
												 texture->UploadHeap));

		mTextures[texture->name] = std::move(texture);
	}

	// water1
	{
		auto texture = std::make_unique<Texture>();
		texture->name = "water1";
#if RENDERDOC_BUILD
		texture->filename = L"../../../../textures/water1.dds";
#else // RENDERDOC_BUILD
		texture->filename = L"../../textures/water1.dds";
#endif // RENDERDOC_BUILD

		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
												 mCommandList.Get(),
												 texture->filename.c_str(),
												 texture->resource,
												 texture->UploadHeap));

		mTextures[texture->name] = std::move(texture);
	}

	// WoodCrate01
	{
		auto texture = std::make_unique<Texture>();
		texture->name = "WireFence";
#if RENDERDOC_BUILD
		texture->filename = L"../../../../textures/WireFence.dds";
#else // RENDERDOC_BUILD
		texture->filename = L"../../textures/WireFence.dds";
#endif // RENDERDOC_BUILD

		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
												 mCommandList.Get(),
												 texture->filename.c_str(),
												 texture->resource,
												 texture->UploadHeap));

		mTextures[texture->name] = std::move(texture);
	}
}

void ApplicationInstance::BuildRootSignatures()
{
	CD3DX12_DESCRIPTOR_RANGE TextureTable;
	TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER params[4];

	params[0].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_ALL);
	params[1].InitAsConstantBufferView(0);
	params[2].InitAsConstantBufferView(1);
	params[3].InitAsConstantBufferView(2);
	//params[4].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_ALL);
	
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

	mBlur->BuildRootSignature();
	mSobel->BuildRootSignature(GetStaticSamplers());
}

void ApplicationInstance::BuildDescriptorHeaps()
{
	std::vector<std::pair<std::string, D3D12_SRV_DIMENSION>> textures =
	{
		{ "grass", D3D12_SRV_DIMENSION_TEXTURE2D },
		{ "water1", D3D12_SRV_DIMENSION_TEXTURE2D },
		{ "WireFence", D3D12_SRV_DIMENSION_TEXTURE2D },
	};

	// create SRV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = textures.size() + mBlur->DescriptorCount() + mSobel->DescriptorCount() + mOffscreenRT->DescriptorCount();
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

	auto SrvCpuStart = mCBVSRVUAVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto SrvGpuStart = mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto RtvCpuStart = mRTVHeap->GetCPUDescriptorHandleForHeapStart();

	INT SrvCpuOffset = textures.size();
	INT SrvGpuOffset = textures.size();
	INT RtvCpuOffset = SwapChainBufferSize;

	mBlur->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(SrvCpuStart, SrvCpuOffset, mCBVSRVUAVDescriptorSize),
							CD3DX12_GPU_DESCRIPTOR_HANDLE(SrvGpuStart, SrvGpuOffset, mCBVSRVUAVDescriptorSize),
							mCBVSRVUAVDescriptorSize);

	SrvCpuOffset += mBlur->DescriptorCount();
	SrvGpuOffset += mBlur->DescriptorCount();

	mSobel->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(SrvCpuStart, SrvCpuOffset, mCBVSRVUAVDescriptorSize),
							 CD3DX12_GPU_DESCRIPTOR_HANDLE(SrvGpuStart, SrvGpuOffset, mCBVSRVUAVDescriptorSize),
							 mCBVSRVUAVDescriptorSize);

	SrvCpuOffset += mSobel->DescriptorCount();
	SrvGpuOffset += mSobel->DescriptorCount();

	mOffscreenRT->BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE(SrvCpuStart, SrvCpuOffset, mCBVSRVUAVDescriptorSize),
								   CD3DX12_GPU_DESCRIPTOR_HANDLE(SrvGpuStart, SrvGpuOffset, mCBVSRVUAVDescriptorSize),
								   CD3DX12_CPU_DESCRIPTOR_HANDLE(RtvCpuStart, RtvCpuOffset, mRTVDescriptorSize));
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
	mShaders["HorzBlurCS"] = Utils::CompileShader(L"../../shaders/blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
	mShaders["VertBlurCS"] = Utils::CompileShader(L"../../shaders/blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");
#else // RENDERDOC_BUILD
	mShaders["VS"] = Utils::CompileShader(L"shaders/default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["PS"] = Utils::CompileShader(L"shaders/default.hlsl", defines, "PS", "ps_5_0");
	mShaders["AlphaTestedPS"] = Utils::CompileShader(L"shaders/default.hlsl", AlphaTestDefines, "PS", "ps_5_0");
	mShaders["HorzBlurCS"] = Utils::CompileShader(L"shaders/blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
	mShaders["VertBlurCS"] = Utils::CompileShader(L"shaders/blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");
	mShaders["SobelCS"] = Utils::CompileShader(L"shaders/sobel.hlsl", nullptr, "CS", "cs_5_0");
	mShaders["CompositeVS"] = Utils::CompileShader(L"shaders/composite.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["CompositePS"] = Utils::CompileShader(L"shaders/composite.hlsl", nullptr, "PS", "ps_5_0");
#endif // RENDERDOC_BUILD

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void ApplicationInstance::BuildMeshGeometry()
{
	GeometryGenerator generator;

	// land
	{
		GeometryGenerator::MeshData grid = generator.CreateGrid(160.0f, 160.0f, 50, 50);

		std::vector<Vertex> vertices(grid.vertices.size());
		std::vector<uint16_t> indices = grid.GetIndices16();

		for (size_t i = 0; i < grid.vertices.size(); ++i)
		{
			Vertex& vertex = vertices[i];

			vertex.position = grid.vertices[i].position;
			vertex.position.y = GetHillHeight(vertex.position.x, vertex.position.z);
			vertex.normal = GetHillNormal(vertex.position.x, vertex.position.z);
			vertex.TexCoord = grid.vertices[i].TexCoord;
		}

		UINT VertexBufferByteSize = vertices.size() * sizeof(Vertex);
		UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

		auto geometry = std::make_unique<MeshGeometry>();
		geometry->name = "land";

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

		SubMeshGeometry SubMesh;
		SubMesh.IndexCount = indices.size();
		SubMesh.StartIndexLocation = 0;
		SubMesh.BaseVertexLocation = 0;

		geometry->DrawArgs["grid"] = SubMesh;

		mMeshGeometries[geometry->name] = std::move(geometry);
	}

	// water
	{
		std::vector<uint16_t> indices(3 * mWaves->TriangleCount());
		assert(mWaves->VertexCount() < 0x0000ffff);

		int m = mWaves->RowCount();
		int n = mWaves->ColCount();
		int k = 0;

		for (int i = 0; i < m - 1; ++i)
		{
			for (int j = 0; j < n - 1; ++j)
			{
				indices[k + 0] = (i + 0) * n + (j + 0);
				indices[k + 1] = (i + 0) * n + (j + 1);
				indices[k + 2] = (i + 1) * n + (j + 0);

				indices[k + 3] = (i + 1) * n + (j + 0);
				indices[k + 4] = (i + 0) * n + (j + 1);
				indices[k + 5] = (i + 1) * n + (j + 1);

				k += 6;
			}
		}

		UINT VertexBufferByteSize = mWaves->VertexCount() * sizeof(Vertex);
		UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

		auto geometry = std::make_unique<MeshGeometry>();
		geometry->name = "water";

		// set dynamically
		geometry->VertexBufferCPU = nullptr;
		geometry->VertexBufferGPU = nullptr;

		ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &geometry->IndexBufferCPU));
		CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);

		geometry->IndexBufferGPU = Utils::CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), indices.data(), IndexBufferByteSize, geometry->IndexBufferUploader);

		geometry->VertexByteStride = sizeof(Vertex);
		geometry->VertexBufferByteSize = VertexBufferByteSize;
		geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
		geometry->IndexBufferByteSize = IndexBufferByteSize;

		SubMeshGeometry SubMesh;
		SubMesh.IndexCount = indices.size();
		SubMesh.StartIndexLocation = 0;
		SubMesh.BaseVertexLocation = 0;

		geometry->DrawArgs["grid"] = SubMesh;

		mMeshGeometries[geometry->name] = std::move(geometry);
	}

	// box
	{
		GeometryGenerator::MeshData mesh = generator.CreateBox(8.0f, 8.0f, 8.0f, 3);

		SubMeshGeometry SubMesh;
		SubMesh.IndexCount = mesh.indices32.size();
		SubMesh.StartIndexLocation = 0;
		SubMesh.BaseVertexLocation = 0;

		std::vector<Vertex> vertices(mesh.vertices.size());
		std::vector<uint16_t> indices = mesh.GetIndices16();

		for (size_t i = 0; i < mesh.vertices.size(); ++i)
		{
			Vertex& vertex = vertices[i];

			vertex.position = mesh.vertices[i].position;
			vertex.normal = mesh.vertices[i].normal;
			vertex.TexCoord = mesh.vertices[i].TexCoord;
		}

		UINT VertexBufferByteSize = vertices.size() * sizeof(Vertex);
		UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

		auto geometry = std::make_unique<MeshGeometry>();
		geometry->name = "box";

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

		geometry->DrawArgs["box"] = SubMesh;

		mMeshGeometries[geometry->name] = std::move(geometry);
	}
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

	// alpha tested
	{
		descs["alpha_tested"] = descs["opaque"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["alpha_tested"];

		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["AlphaTestedPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["AlphaTestedPS"]->GetBufferSize();
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["alpha_tested"])));
	}

	// horizontal blur
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = mBlur->GetRootSignature();
		desc.CS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["HorzBlurCS"]->GetBufferPointer());
		desc.CS.BytecodeLength = mShaders["HorzBlurCS"]->GetBufferSize();
		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ThrowIfFailed(mDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["horz_blur"])));
	}

	// vertical blur
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = mBlur->GetRootSignature();
		desc.CS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["VertBlurCS"]->GetBufferPointer());
		desc.CS.BytecodeLength = mShaders["VertBlurCS"]->GetBufferSize();
		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ThrowIfFailed(mDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["vert_blur"])));
	}

	// sobel
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = mSobel->GetRootSignature();
		desc.CS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["SobelCS"]->GetBufferPointer());
		desc.CS.BytecodeLength = mShaders["SobelCS"]->GetBufferSize();
		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		
		ThrowIfFailed(mDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["sobel"])));
	}

	// composite
	{
		descs["composite"] = descs["opaque"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["composite"];

		desc.pRootSignature = mSobel->GetRootSignature();
		// disable depth test
		desc.DepthStencilState.DepthEnable = false;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["CompositeVS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["CompositeVS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["CompositePS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["CompositePS"]->GetBufferSize();

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["composite"])));
	}
}

void ApplicationInstance::BuildFrameResources()
{
	for (int i = 0; i < gFrameResourcesCount; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(), 1, mRenderItems.size(), mMaterials.size(), mWaves->VertexCount()));
	}
}

void ApplicationInstance::BuildMaterials()
{
	UINT ConstantBufferIndex = 0;

	// grass
	{
		auto material = std::make_unique<Material>();
		material->name = "grass";
		material->ConstantBufferIndex = ConstantBufferIndex++;
		material->DiffuseSRVHeapIndex = material->ConstantBufferIndex;
		material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
		material->roughness = 0.125f;

		mMaterials[material->name] = std::move(material);
	}

	// water
	{
		auto material = std::make_unique<Material>();
		material->name = "water";
		material->ConstantBufferIndex = ConstantBufferIndex++;
		material->DiffuseSRVHeapIndex = material->ConstantBufferIndex;
		material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
		material->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
		material->roughness = 0.0f;

		mMaterials[material->name] = std::move(material);
	}

	// wire fence
	{
		auto material = std::make_unique<Material>();
		material->name = "WireFence";
		material->ConstantBufferIndex = ConstantBufferIndex++;
		material->DiffuseSRVHeapIndex = material->ConstantBufferIndex;
		material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
		material->roughness = 0.25f;

		mMaterials[material->name] = std::move(material);
	}
}

void ApplicationInstance::BuildRenderItems()
{
	UINT ObjectCBIndex = 0;

	// land
	{
		auto item = std::make_unique<RenderItem>();

		item->world = MathHelper::Identity4x4();
		XMStoreFloat4x4(&item->TexCoordTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["land"].get();
		item->material = mMaterials["grass"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["grid"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["grid"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["grid"].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(RenderLayer::opaque)].push_back(item.get());

		mRenderItems.push_back(std::move(item));
	}

	// water
	{
		auto item = std::make_unique<RenderItem>();

		item->world = MathHelper::Identity4x4();
		XMStoreFloat4x4(&item->TexCoordTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["water"].get();
		item->material = mMaterials["water"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["grid"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["grid"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["grid"].BaseVertexLocation;

		mWavesRenderItem = item.get();

		mLayerRenderItems[static_cast<int>(RenderLayer::transparent)].push_back(item.get());

		mRenderItems.push_back(std::move(item));
	}

	// wire fence
	{
		auto item = std::make_unique<RenderItem>();

		XMStoreFloat4x4(&item->world, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
		item->TexCoordTransform = MathHelper::Identity4x4();
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries["box"].get();
		item->material = mMaterials["WireFence"].get();
		item->PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs["box"].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs["box"].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs["box"].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(RenderLayer::AlphaTested)].push_back(item.get());

		mRenderItems.push_back(std::move(item));
	}
}

void ApplicationInstance::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems)
{
	UINT ObjectCBByteSize = Utils::GetConstantBufferByteSize(sizeof(ObjectConstants));
	auto ObjectCB = mCurrentFrameResource->ObjectCB->GetResource();

	UINT MaterialCBByteSize = Utils::GetConstantBufferByteSize(sizeof(MaterialConstants));
	auto MaterialCB = mCurrentFrameResource->MaterialCB->GetResource();

	for (const auto& item : RenderItems)
	{
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView = item->geometry->GetVertexBufferView();
		CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
		D3D12_INDEX_BUFFER_VIEW IndexBufferView = item->geometry->GetIndexBufferView();
		CommandList->IASetIndexBuffer(&IndexBufferView);

		CommandList->IASetPrimitiveTopology(item->PrimitiveTopology);

		CD3DX12_GPU_DESCRIPTOR_HANDLE srv(mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		srv.Offset(item->material->DiffuseSRVHeapIndex, mCBVSRVUAVDescriptorSize);
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

void ApplicationInstance::DrawFullscreenQuad(ID3D12GraphicsCommandList* CommandList)
{
	//CommandList->IASetVertexBuffers(0, 1, nullptr);
	CommandList->IASetIndexBuffer(nullptr);
	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CommandList->DrawInstanced(6, 1, 0, 0);
}

float ApplicationInstance::GetHillHeight(float x, float z) const
{
	return 0.3f * (z * std::sin(0.1f * x) + x * std::cos(0.1f * z));
}

XMFLOAT3 ApplicationInstance::GetHillNormal(float x, float z) const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n = XMFLOAT3(-0.03f * z * std::cos(0.1f * x) - 0.3f * std::cos(0.1f * z),
						  1.0f,
						  -0.3f * std::sin(0.1f * x) + 0.03f * x * std::sin(0.1f * z));

	XMStoreFloat3(&n, XMVector3Normalize(XMLoadFloat3(&n)));
	return n;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>& ApplicationInstance::GetStaticSamplers()
{
	static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> StaticSamplers = []() -> std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>
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