#include "ApplicationFramework.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "camera.h"
#include "ShadowMap.h"
#include "SSAO.h"

#include <numeric>
#include <sstream>
#include <fstream>

#define RENDERDOC_BUILD 0

#if RENDERDOC_BUILD
#define PREFIX(str) std::wstring(L"../../") + str
#else // RENDERDOC_BUILD
#define PREFIX(str) str
#endif // RENDERDOC_BUILD

const UINT kFrameResourcesCount = 3;

using samplers = std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7>;

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 world = MathHelper::Identity4x4();
	XMFLOAT4X4 TexCoordTransform = MathHelper::Identity4x4();

	int DirtyFramesCount = kFrameResourcesCount;

	UINT ConstantBufferIndex = -1;

	MeshGeometry* geometry = nullptr;
	Material* material = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	BoundingBox bounds;
	//bool bIsVisible = true;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	opaque = 0,
	debug,
	sky,
	count
};

class ApplicationInstance : public ApplicationFramework
{
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrentFrameResource = nullptr;
	int mCurrentFrameResourceIndex = 0;

	UINT mCBVSRVUAVDescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mAmbientOcclusionRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCBVSRVUAVDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mMeshGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPipelineStateObjects;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::vector<std::unique_ptr<RenderItem>> mRenderItems;
	std::vector<RenderItem*> mLayerRenderItems[static_cast<int>(RenderLayer::count)];

	UINT mSkyTextureHeapIndex = 0;
	UINT mShadowMapTextureHeapIndex = 0;
	UINT mAmbientOcclusionTextureHeapIndex = 0;

	MainPassConstants mMainPassCB;
	MainPassConstants mShadowPassCB;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSRV;

	Camera mCamera;
	//BoundingFrustum mCameraFrustum;
	//bool mIsFrustumCullingEnabled = true;

	std::unique_ptr<ShadowMap> mShadowMap;
	DirectX::BoundingSphere mSceneBounds;
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPositionW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] =
	{
		XMFLOAT3(+0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];

	std::unique_ptr<SSAO> mSSAO;

	bool mIsWireFrameEnabled = false;
	bool mIsNormalMappingEnabled = false;
	bool mIsShadowMappingEnabled = false;
	bool mIsShadowDebugViewEnabled = false;
	bool mIsAmbientOcclusionEnabled = false;
	bool mIsAmbientOcclusionDebugViewEnabled = false;

	POINT mLastMousePosition = { 0, 0 };

	virtual void CreateRTVAndDSVDescriptorHeaps() override;

	virtual void OnResize() override;
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;

	virtual void OnMouseDown(WPARAM state, int x, int y) override;
	virtual void OnMouseUp(WPARAM state, int x, int y) override;
	virtual void OnMouseMove(WPARAM state, int x, int y) override;
	void OnKeyboardEvent(const GameTimer& timer);

	void AnimateMaterials(const GameTimer& timer);
	void UpdateObjectCBs(const GameTimer& timer);
	void UpdateMaterialBuffer(const GameTimer& timer);
	void UpdateShadowTransform(const GameTimer& timer);
	void UpdateMainPassCB(const GameTimer& timer);
	void UpdateShadowPassCB(const GameTimer& timer);
	void UpdateAmbientOcclusionCB(const GameTimer& timer);

	void BuildRootSignatures();
	void BuildAmbientOcclusionRootSignatures();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildSceneGeometry();
	void BuildSkullGeometry();
	void BuildPipelineStateObjects();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();

	void LoadTextures();
	const samplers& GetStaticSamplers();

	void DrawRenderItems(ID3D12GraphicsCommandList* CommandList,
						 const std::vector<RenderItem*>& RenderItems);

	void DrawSceneToShadowMap();
	void DrawNormalsAndDepth();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSRV(const int index) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSRV(const int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDSV(const int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTV(const int index) const;

public:
	ApplicationInstance(HINSTANCE instance);
	~ApplicationInstance();

	virtual bool init() override;
};

ApplicationInstance::ApplicationInstance(HINSTANCE instance)
	: ApplicationFramework(instance)
{
	// scene bounds based on the grid mesh
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = std::sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
}

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

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);

	mShadowMap = std::make_unique<ShadowMap>(mDevice.Get(), 2048, 2048);

	mSSAO = std::make_unique<SSAO>(mDevice.Get(),
								   mCommandList.Get(),
								   mMainWindowWidth, mMainWindowHeight);

	LoadTextures();
	BuildRootSignatures();
	BuildAmbientOcclusionRootSignatures();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildSceneGeometry();
	BuildSkullGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPipelineStateObjects();

	mSSAO->SetPSOs(mPipelineStateObjects["AmbientOcclusion"].Get(),
				   mPipelineStateObjects["AmbientOcclusionBlur"].Get());

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
		desc.NumDescriptors = SwapChainBufferSize + 3; // +1 normals +1 ambient map +1 blur extra ambient map
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mRTVHeap.GetAddressOf())));
	}

	// DSV
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.NumDescriptors = 1 + 1; // +1 shadow map
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

	mCamera.SetLens(0.25f * XM_PI, GetAspectRatio(), 1.0f, 1000.0f);
	//BoundingFrustum::CreateFromMatrix(mCameraFrustum, mCamera.GetProj());

	if (mSSAO != nullptr)
	{
		mSSAO->OnResize(mMainWindowWidth, mMainWindowHeight);
		mSSAO->RebuildDescriptors(mDepthStencilBuffer.Get());
	}
}

void ApplicationInstance::update(GameTimer& timer)
{
	OnKeyboardEvent(timer);

	mCurrentFrameResourceIndex = (mCurrentFrameResourceIndex + 1) % kFrameResourcesCount;
	mCurrentFrameResource = mFrameResources[mCurrentFrameResourceIndex].get();

	if (mCurrentFrameResource->fence != 0 && mFence->GetCompletedValue() < mCurrentFrameResource->fence)
	{
		HANDLE handle = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameResource->fence, handle));
		WaitForSingleObject(handle, INFINITE);
		CloseHandle(handle);
	}

	// animate lights
	{
		mLightRotationAngle += 0.1f * timer.GetDeltaTime();

		XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
		for (int i = 0; i < 3; ++i)
		{
			XMVECTOR dir = XMLoadFloat3(&mBaseLightDirections[i]);
			dir = XMVector3TransformNormal(dir, R);
			XMStoreFloat3(&mRotatedLightDirections[i], dir);
		}
	}

	AnimateMaterials(timer);
	UpdateObjectCBs(timer);
	UpdateMaterialBuffer(timer);
	UpdateShadowTransform(timer);
	UpdateMainPassCB(timer);
	UpdateShadowPassCB(timer);
	UpdateAmbientOcclusionCB(timer);
}

void ApplicationInstance::draw(GameTimer& timer)
{
#if IS_IMGUI_ENABLED
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("settings");

		//ImGui::DragInt("blur count", &mBlur->GetCount(), 1, 0, 4);

		ImGui::End();
	}

	ImGui::Render();
#endif // IS_IMGUI_ENABLED

	auto CommandAllocator = mCurrentFrameResource->CommandAllocator;

	ThrowIfFailed(CommandAllocator->Reset());

	ThrowIfFailed(mCommandList->Reset(CommandAllocator.Get(), mPipelineStateObjects["opaque"].Get()));

	{
		ID3D12DescriptorHeap* heaps[] = { mCBVSRVUAVDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	}

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// bind all materials
	auto MaterialBuffer = mCurrentFrameResource->MaterialBuffer->GetResource();
	mCommandList->SetGraphicsRootShaderResourceView(2, MaterialBuffer->GetGPUVirtualAddress());

	// bind all diffuse/normal textures
	mCommandList->SetGraphicsRootDescriptorTable(4, mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// ==================== SHADOW PASS ====================

	// bind null cube map and null shadom map textures for shadow pass
	mCommandList->SetGraphicsRootDescriptorTable(3, mNullSRV);

	DrawSceneToShadowMap();

	// ==================== NORMAL/DEPTH PASS ====================

	DrawNormalsAndDepth();

	// ==================== SSAO PASS ====================

	mCommandList->SetGraphicsRootSignature(mAmbientOcclusionRootSignature.Get());
	mSSAO->ComputeAmbientOcclusion(mCommandList.Get(), mCurrentFrameResource, 3);

	// ==================== MAIN PASS ====================

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// rebind state whenever graphics root signature changes
	{
		// bind all materials
		auto MaterialBuffer = mCurrentFrameResource->MaterialBuffer->GetResource();
		mCommandList->SetGraphicsRootShaderResourceView(2, MaterialBuffer->GetGPUVirtualAddress());

		// bind all diffuse/normal textures
		mCommandList->SetGraphicsRootDescriptorTable(4, mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
																				   D3D12_RESOURCE_STATE_PRESENT,
																				   D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	//mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);
	}

	// rebind diffuse/normal textures ?

	// bind main pass constant buffer
	auto MainPassCB = mCurrentFrameResource->MainPassCB->GetResource();
	mCommandList->SetGraphicsRootConstantBufferView(1, MainPassCB->GetGPUVirtualAddress());
	
	// bind sky cube map and shadow map textures
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE SkyTextureDescriptor(mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		SkyTextureDescriptor.Offset(mSkyTextureHeapIndex, mCBVSRVUAVDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(3, SkyTextureDescriptor);
	}

	// draw scene
	const std::string state = mIsWireFrameEnabled ? "opaque_wireframe" : mIsNormalMappingEnabled ? "opaque_normal_mapping" : "opaque";
	mCommandList->SetPipelineState(mPipelineStateObjects[state].Get());
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::opaque)]);

	if (mIsShadowDebugViewEnabled)
	{
		// draw debug
		mCommandList->SetPipelineState(mPipelineStateObjects["debug"].Get()); // shadow or ambient
		DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::debug)]);
	}

	// draw sky
	mCommandList->SetPipelineState(mPipelineStateObjects[mIsWireFrameEnabled ? "sky_wireframe" : "sky"].Get());
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::sky)]);

#if IS_IMGUI_ENABLED
	// imgui
	{
		ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	}
#endif // IS_IMGUI_ENABLED

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
																				   D3D12_RESOURCE_STATE_RENDER_TARGET,
																				   D3D12_RESOURCE_STATE_PRESENT);
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

		mCamera.pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePosition.x = x;
	mLastMousePosition.y = y;
}

void ApplicationInstance::OnKeyboardEvent(const GameTimer& timer)
{
	mIsWireFrameEnabled = (GetAsyncKeyState('1') & 0x8000);
	mIsNormalMappingEnabled = !(GetAsyncKeyState('2') & 0x8000);
	mIsShadowMappingEnabled = !(GetAsyncKeyState('3') & 0x8000);
	mIsShadowDebugViewEnabled = (GetAsyncKeyState('4') & 0x8000);
	mIsAmbientOcclusionEnabled = !(GetAsyncKeyState('5') & 0x8000);
	mIsAmbientOcclusionDebugViewEnabled = (GetAsyncKeyState('6') & 0x8000);

	const float dt = timer.GetDeltaTime();

	if ((GetAsyncKeyState('W') & 0x8000))
	{
		mCamera.walk(+10.0f * dt);
	}
	if ((GetAsyncKeyState('S') & 0x8000))
	{
		mCamera.walk(-10.0f * dt);
	}
	if ((GetAsyncKeyState('A') & 0x8000))
	{
		mCamera.strafe(-10.0f * dt);
	}
	if ((GetAsyncKeyState('D') & 0x8000))
	{
		mCamera.strafe(+10.0f * dt);
	}

	//if (GetAsyncKeyState('1') & 0x8000)
	//{
	//	mIsFrustumCullingEnabled = true;
	//}

	//if (GetAsyncKeyState('2') & 0x8000)
	//{
	//	mIsFrustumCullingEnabled = false;
	//}

	mCamera.UpdateViewMatrix();
}

void ApplicationInstance::AnimateMaterials(const GameTimer& timer)
{}

void ApplicationInstance::UpdateObjectCBs(const GameTimer& timer)
{
	//const XMMATRIX view = mCamera.GetView();
	//const XMMATRIX ViewInverse = MathHelper::GetMatrixInverse(view);

	//auto CurrentInstanceBuffer = mCurrentFrameResource->InstanceBuffer.get();

	//for (auto& object : mRenderItems)
	//{
	//	UINT VisibleInstanceCount = 0;

	//	for (const InstanceData& instance : object->instances)
	//	{
	//		const XMMATRIX world = XMLoadFloat4x4(&instance.world);
	//		const XMMATRIX WorldInverse = MathHelper::GetMatrixInverse(world);
	//		const XMMATRIX TexCoordTransform = XMLoadFloat4x4(&instance.TexCoordTransform);

	//		// view space to the object's local space
	//		const XMMATRIX ViewToLocal = XMMatrixMultiply(ViewInverse, WorldInverse);

	//		// transform the camera frustum from view space to the object's local space
	//		BoundingFrustum LocalSpaceFrustum;
	//		mCameraFrustum.Transform(LocalSpaceFrustum, ViewToLocal);

	//		// box-frustum intersection test in local space
	//		if ((LocalSpaceFrustum.Contains(object->bounds) != DirectX::DISJOINT) || (mIsFrustumCullingEnabled == false))
	//		{
	//			InstanceData data;
	//			XMStoreFloat4x4(&data.world, XMMatrixTranspose(world));
	//			XMStoreFloat4x4(&data.TexCoordTransform, XMMatrixTranspose(TexCoordTransform));
	//			data.MaterialIndex = instance.MaterialIndex;

	//			// write the instance data to structured buffer for the visible objects
	//			CurrentInstanceBuffer->CopyData(VisibleInstanceCount++, data);
	//		}
	//	}

	//	object->InstanceCount = VisibleInstanceCount;

	//	std::wostringstream stream;
	//	stream.precision(6);
	//	stream <<	L"Instancing and Frustum Culling" <<
	//				L"    " << object->InstanceCount <<
	//				L" objects visible out of " << object->instances.size();
	//	mMainWindowTitle = stream.str();
	//}

	auto CurrentObjectCB = mCurrentFrameResource->ObjectCB.get();

	for (auto& object : mRenderItems)
	{
		if (object->DirtyFramesCount > 0)
		{
			const XMMATRIX world = XMLoadFloat4x4(&object->world);
			const XMMATRIX TexCoordTransform = XMLoadFloat4x4(&object->TexCoordTransform);

			ObjectConstants buffer;
			XMStoreFloat4x4(&buffer.world, XMMatrixTranspose(world));
			XMStoreFloat4x4(&buffer.TexCoordTransform, XMMatrixTranspose(TexCoordTransform));
			buffer.MaterialIndex = object->material->ConstantBufferIndex;

			CurrentObjectCB->CopyData(object->ConstantBufferIndex, buffer);

			object->DirtyFramesCount--;
		}
	}
}

void ApplicationInstance::UpdateMaterialBuffer(const GameTimer& timer)
{
	auto CurrentMaterialBuffer = mCurrentFrameResource->MaterialBuffer.get();

	for (auto& [key, material] : mMaterials)
	{
		if (material->DirtyFramesCount > 0)
		{
			XMMATRIX MaterialTransform = XMLoadFloat4x4(&material->transform);

			MaterialData buffer;
			buffer.DiffuseAlbedo = material->DiffuseAlbedo;
			buffer.FresnelR0 = material->FresnelR0;
			buffer.roughness = material->roughness;
			XMStoreFloat4x4(&buffer.transform, XMMatrixTranspose(MaterialTransform));
			buffer.DiffuseTextureIndex = material->DiffuseSRVHeapIndex;
			buffer.NormalTextureIndex = material->NormalSRVHeapIndex;

			CurrentMaterialBuffer->CopyData(material->ConstantBufferIndex, buffer);

			(material->DirtyFramesCount)--;
		}
	}
}

void ApplicationInstance::UpdateShadowTransform(const GameTimer& timer)
{
	// only the first "main" light casts a shadow
	const XMVECTOR direction = XMLoadFloat3(&mRotatedLightDirections[0]);
	const XMVECTOR position = -2.0f * mSceneBounds.Radius * direction;
	const XMVECTOR target = XMLoadFloat3(&mSceneBounds.Center);
	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMMATRIX view = XMMatrixLookAtLH(position, target, up);

	XMStoreFloat3(&mLightPositionW, position);

	// transform bounding sphere to light space
	XMFLOAT3 center;
	XMStoreFloat3(&center, XMVector3TransformCoord(target, view));

	// ortho frustum in light space encloses scene
	const float l = center.x - mSceneBounds.Radius;
	const float b = center.y - mSceneBounds.Radius;
	const float n = center.z - mSceneBounds.Radius;
	const float r = center.x + mSceneBounds.Radius;
	const float t = center.y + mSceneBounds.Radius;
	const float f = center.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;
	const XMMATRIX proj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// transform NDC space [-1,+1]^2 to texture space [0,1]^2
	const XMMATRIX T(0.5f,  0.0f, 0.0f, 0.0f,
					 0.0f, -0.5f, 0.0f, 0.0f,
					 0.0f,  0.0f, 1.0f, 0.0f,
					 0.5f,  0.5f, 0.0f, 1.0f);

	const XMMATRIX S = view * proj * T;
	XMStoreFloat4x4(&mLightView, view);
	XMStoreFloat4x4(&mLightProj, proj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void ApplicationInstance::UpdateMainPassCB(const GameTimer& timer)
{
	const XMMATRIX view = mCamera.GetView();
	const XMMATRIX ViewInverse = MathHelper::GetMatrixInverse(view);
	const XMMATRIX proj = mCamera.GetProj();
	const XMMATRIX ProjInverse = MathHelper::GetMatrixInverse(proj);
	const XMMATRIX ViewProj = view * proj;
	const XMMATRIX ViewProjInverse = MathHelper::GetMatrixInverse(ViewProj);

	XMStoreFloat4x4(&mMainPassCB.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.ViewInverse, XMMatrixTranspose(ViewInverse));
	XMStoreFloat4x4(&mMainPassCB.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.ProjInverse, XMMatrixTranspose(ProjInverse));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjInverse, XMMatrixTranspose(ViewProjInverse));

	XMMATRIX ShadowTransform = XMLoadFloat4x4(&mShadowTransform);
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(ShadowTransform));

	mMainPassCB.EyePositionWorld = mCamera.GetPositionF();
	mMainPassCB.RenderTargetSize = XMFLOAT2(mMainWindowWidth, mMainWindowHeight);
	mMainPassCB.RenderTargetSizeInverse = XMFLOAT2(1.0f / mMainWindowWidth, 1.0f / mMainWindowHeight);
	mMainPassCB.NearPlane = 1.0f;
	mMainPassCB.FarPlane = 1000.0f;
	mMainPassCB.DeltaTime = timer.GetDeltaTime();
	mMainPassCB.TotalTime = timer.GetTotalTime();

	mMainPassCB.AmbientLight = { 0.4f, 0.4f, 0.6f, 1.0f };

	mMainPassCB.lights[0].direction = mRotatedLightDirections[0];
	mMainPassCB.lights[0].strength = { 0.4f, 0.4f, 0.5f };
	mMainPassCB.lights[1].direction = mRotatedLightDirections[1];
	mMainPassCB.lights[1].strength = { 0.1f, 0.1f, 0.1f };
	mMainPassCB.lights[2].direction = mRotatedLightDirections[2];
	mMainPassCB.lights[2].strength = { 0.0f, 0.0f, 0.0f };

	auto CurrentMainPassCB = mCurrentFrameResource->MainPassCB.get();
	CurrentMainPassCB->CopyData(0, mMainPassCB);
}

void ApplicationInstance::UpdateShadowPassCB(const GameTimer& timer)
{
	const XMMATRIX view = XMLoadFloat4x4(&mLightView);
	const XMMATRIX ViewInverse = MathHelper::GetMatrixInverse(view);
	const XMMATRIX proj = XMLoadFloat4x4(&mLightProj);
	const XMMATRIX ProjInverse = MathHelper::GetMatrixInverse(proj);
	const XMMATRIX ViewProj = view * proj;
	const XMMATRIX ViewProjInverse = MathHelper::GetMatrixInverse(ViewProj);

	const UINT w = mShadowMap->GetWidth();
	const UINT h = mShadowMap->GetHeight();

	XMStoreFloat4x4(&mShadowPassCB.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.ViewInverse, XMMatrixTranspose(ViewInverse));
	XMStoreFloat4x4(&mShadowPassCB.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.ProjInverse, XMMatrixTranspose(ProjInverse));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(ViewProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProjInverse, XMMatrixTranspose(ViewProjInverse));
	mShadowPassCB.EyePositionWorld = mLightPositionW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2(w, h);
	mShadowPassCB.RenderTargetSizeInverse = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearPlane = mLightNearZ;
	mShadowPassCB.FarPlane = mLightFarZ;

	auto CurrentShadowPassCB = mCurrentFrameResource->MainPassCB.get();
	CurrentShadowPassCB->CopyData(1, mShadowPassCB);
}

void ApplicationInstance::UpdateAmbientOcclusionCB(const GameTimer& timer)
{
	AmbientOcclusionConstants buffer;

	const XMMATRIX P = mCamera.GetProj();
	
	// transform NDC space [-1,+1]^2 to texture space [0,1]^2
	const XMMATRIX T(0.5f,  0.0f, 0.0f, 0.0f,
					 0.0f, -0.5f, 0.0f, 0.0f,
					 0.0f,  0.0f, 1.0f, 0.0f,
					 0.5f,  0.5f, 0.0f, 1.0f);

	buffer.proj = mMainPassCB.proj;
	buffer.ProjInverse = mMainPassCB.ProjInverse;
	XMStoreFloat4x4(&buffer.ProjTex, XMMatrixTranspose(P * T));

	mSSAO->GetOffsetVectors(buffer.OffsetVectors);

	// blur weights
	const auto weights = mSSAO->CalcGaussWeights(2.5f);
	buffer.BlurWeights[0] = XMFLOAT4(&weights[0]);
	buffer.BlurWeights[1] = XMFLOAT4(&weights[4]);
	buffer.BlurWeights[2] = XMFLOAT4(&weights[8]);

	buffer.InvRenderTargetSize = XMFLOAT2(1.0f / mSSAO->GetAmbientMapWidth(),
										  1.0f / mSSAO->GetAmbientMapHeight());

	// coordinates given in view space
	buffer.OcclusionRadius = 0.5f;
	buffer.OcclusionFadeStart = 0.2f;
	buffer.OcclusionFadeEnd = 1.0f;
	buffer.SurfaceEpsilon = 0.05f;

	auto CurrentAmbientOcclusionCB = mCurrentFrameResource->AmbientOcclusionCB.get();
	CurrentAmbientOcclusionCB->CopyData(0, buffer);
}

void ApplicationInstance::LoadTextures()
{
	const auto LoadTexture = [this](const std::string& name)
	{
		// very bad way to convert a narrow string to a wide string
		const std::wstring wname(name.begin(), name.end());

		auto texture = std::make_unique<Texture>();
		texture->name = name;
		texture->filename = PREFIX(L"../textures/") + wname + L".dds";

		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
												 mCommandList.Get(),
												 texture->filename.c_str(),
												 texture->resource,
												 texture->UploadHeap));

		ThrowIfFailed(texture->resource->SetPrivateData(WKPDID_D3DDebugObjectName,
														texture->name.size(),
														texture->name.data()));

		mTextures[texture->name] = std::move(texture);

	};

	const std::array<const std::string, 7> textures =
	{
		"bricks2",
		"bricks2_nmap",
		"tile",
		"tile_nmap",
		"white1x1",
		"default_nmap",
		"sunsetcube1024"
	};

	for (const std::string& texture : textures)
	{
		LoadTexture(texture);
	}
}

void ApplicationInstance::BuildRootSignatures()
{
	// perfomance TIP: order from most frequent to least frequent
	CD3DX12_ROOT_PARAMETER params[5];

	// object constant buffer (b0)
	params[0].InitAsConstantBufferView(0);
	// main pass constant buffer (b1)
	params[1].InitAsConstantBufferView(1);
	// all materials (t0, space1)
	params[2].InitAsShaderResourceView(0, 1);
	// sky cube map, shadow map and ambient map textures (t0, space0)
	{
		CD3DX12_DESCRIPTOR_RANGE TextureTable;
		TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0);
		params[3].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	}
	// all other diffuse and normal textures (t3, space0)
	{
		CD3DX12_DESCRIPTOR_RANGE TextureTable;
		TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, mTextures.size() - 1, 3, 0);
		params[4].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	auto StaticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC desc(sizeof(params) / sizeof(CD3DX12_ROOT_PARAMETER),
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

void ApplicationInstance::BuildAmbientOcclusionRootSignatures()
{
	// perfomance TIP: order from most frequent to least frequent
	CD3DX12_ROOT_PARAMETER params[4];

	// ambient occlusion constant buffer (b0)
	params[0].InitAsConstantBufferView(0);
	// ??? (b1)
	params[1].InitAsConstants(1, 1);
	// normals and depth ??? (t0, space0)
	{
		CD3DX12_DESCRIPTOR_RANGE TextureTable;
		TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);
		params[2].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	}
	// random offsets ??? (t2, space0)
	{
		CD3DX12_DESCRIPTOR_RANGE TextureTable;
		TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		params[3].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	const std::array<const CD3DX12_STATIC_SAMPLER_DESC, 4> samplers =
	{
		// point clamp
		CD3DX12_STATIC_SAMPLER_DESC(0,
									D3D12_FILTER_MIN_MAG_MIP_POINT,
									D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
									D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
									D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		// linear wrap
		CD3DX12_STATIC_SAMPLER_DESC(1,
									D3D12_FILTER_MIN_MAG_MIP_LINEAR,
									D3D12_TEXTURE_ADDRESS_MODE_WRAP,
									D3D12_TEXTURE_ADDRESS_MODE_WRAP,
									D3D12_TEXTURE_ADDRESS_MODE_WRAP),
		// linear clamp
		CD3DX12_STATIC_SAMPLER_DESC(2,
									D3D12_FILTER_MIN_MAG_MIP_LINEAR,
									D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
									D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
									D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
		// depth map sampler
		CD3DX12_STATIC_SAMPLER_DESC(3,
									D3D12_FILTER_MIN_MAG_MIP_LINEAR,
									D3D12_TEXTURE_ADDRESS_MODE_BORDER,
									D3D12_TEXTURE_ADDRESS_MODE_BORDER,
									D3D12_TEXTURE_ADDRESS_MODE_BORDER,
									0.0f,
									0,
									D3D12_COMPARISON_FUNC_LESS_EQUAL,
									D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)
	};

	CD3DX12_ROOT_SIGNATURE_DESC desc(sizeof(params) / sizeof(CD3DX12_ROOT_PARAMETER),
									 params,
									 samplers.size(),
									 samplers.data(),
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
											   IID_PPV_ARGS(mAmbientOcclusionRootSignature.GetAddressOf())));
}

void ApplicationInstance::BuildDescriptorHeaps()
{
	// create SRV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = mTextures.size() + 1 + 5 + 3; // +1 shadow map +1 ambient map +1 null cube map +2 null texture 2D
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mCBVSRVUAVDescriptorHeap.GetAddressOf())));
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor(mCBVSRVUAVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto CreateSRV = [&](ID3D12Resource* texture,
						 const D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2D)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = texture ? texture->GetDesc().Format : DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.ViewDimension = dimension;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		switch (desc.ViewDimension)
		{
			case D3D12_SRV_DIMENSION_TEXTURE2D:
				desc.Texture2D.MostDetailedMip = 0;
				desc.Texture2D.ResourceMinLODClamp = 0.0f;
				desc.Texture2D.MipLevels = texture ? texture->GetDesc().MipLevels : 1;
				break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				desc.Texture2DArray.MostDetailedMip = 0;
				desc.Texture2DArray.MipLevels = texture->GetDesc().MipLevels;
				desc.Texture2DArray.FirstArraySlice = 0;
				desc.Texture2DArray.ArraySize = texture->GetDesc().DepthOrArraySize;
				break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				desc.TextureCube.MostDetailedMip = 0;
				desc.TextureCube.MipLevels = texture ? texture->GetDesc().MipLevels : 1;
				desc.TextureCube.ResourceMinLODClamp = 0.0f;
				break;
		}

		mDevice->CreateShaderResourceView(texture, &desc, descriptor);

		descriptor.Offset(1, mCBVSRVUAVDescriptorSize);
	};

	const std::array<const std::string, 7> textures =
	{
		"bricks2",
		"bricks2_nmap",
		"tile",
		"tile_nmap",
		"white1x1",
		"default_nmap",
		"sunsetcube1024"
	};

	for (const auto& name : textures)
	{
		const auto& texture = mTextures[name]->resource.Get();

		if (name == "sunsetcube1024")
		{
			CreateSRV(texture, D3D12_SRV_DIMENSION_TEXTURECUBE);
			mSkyTextureHeapIndex = mTextures.size() - 1;
		}
		else
		{
			CreateSRV(texture);
		}
	}

	auto srvCpuStart = mCBVSRVUAVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = mDSVHeap->GetCPUDescriptorHandleForHeapStart();

	// shadow map
	{
		mShadowMapTextureHeapIndex = mSkyTextureHeapIndex + 1;

		mShadowMap->BuildDescriptors(GetCpuSRV(mShadowMapTextureHeapIndex),
									 GetGpuSRV(mShadowMapTextureHeapIndex),
									 GetDSV(1));

		descriptor.Offset(1, mCBVSRVUAVDescriptorSize);
	}

	// ambient map
	{
		mAmbientOcclusionTextureHeapIndex = mShadowMapTextureHeapIndex + 1;

		mSSAO->BuildDescriptors(mDepthStencilBuffer.Get(),
								GetCpuSRV(mAmbientOcclusionTextureHeapIndex),
								GetGpuSRV(mAmbientOcclusionTextureHeapIndex),
								GetRTV(SwapChainBufferSize),
								mCBVSRVUAVDescriptorSize,
								mRTVDescriptorSize);

		descriptor.Offset(5, mCBVSRVUAVDescriptorSize);
	}

	// null SRVs
	{
		const UINT kNullCubeMapTextureHeapIndex = mAmbientOcclusionTextureHeapIndex + 5; // +?

		CreateSRV(nullptr, D3D12_SRV_DIMENSION_TEXTURECUBE);	// null cube map
		CreateSRV(nullptr);										// 1st null texture 2D
		CreateSRV(nullptr);										// 2nd null texture 2D

		mNullSRV = GetGpuSRV(kNullCubeMapTextureHeapIndex);
	}
}

void ApplicationInstance::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO AlphaTest[] =
	{
		"ALPHA_TEST", "1",
		nullptr, nullptr
	};

	const D3D_SHADER_MACRO NormalMapping[] =
	{
		"NORMAL_MAPPING", "1",
		nullptr, nullptr
	};

	using ShaderData = std::tuple<const std::string,		// name
								  const std::wstring,		// path
								  const D3D_SHADER_MACRO*,	// defines
								  const std::string,		// entry point
								  const std::string>;		// target

	const std::vector<ShaderData> shaders =
	{
		{"VS", L"shaders/default.hlsl", nullptr, "VS", "vs_5_1"},
		{"PS", L"shaders/default.hlsl", nullptr, "PS", "ps_5_1"},
		{"NormalMappingPS", L"shaders/default.hlsl", NormalMapping, "PS", "ps_5_1"},

		{"ShadowVS", L"shaders/shadow.hlsl", nullptr, "VS", "vs_5_1"},
		{"ShadowPS", L"shaders/shadow.hlsl", nullptr, "PS", "ps_5_1"},
		{"ShadowAlphaTestPS", L"shaders/shadow.hlsl", AlphaTest, "PS", "ps_5_1"},

		{"DebugVS", L"shaders/debug.hlsl", nullptr, "VS", "vs_5_1"},
		{"DebugPS", L"shaders/debug.hlsl", nullptr, "PS", "ps_5_1"},

		{"NormalsVS", L"shaders/normals.hlsl", nullptr, "VS", "vs_5_1"},
		{"NormalsPS", L"shaders/normals.hlsl", nullptr, "PS", "ps_5_1"},

		{"AmbientOcclusionVS", L"shaders/ssao.hlsl", nullptr, "VS", "vs_5_1"},
		{"AmbientOcclusionPS", L"shaders/ssao.hlsl", nullptr, "PS", "ps_5_1"},
		{"AmbientOcclusionBlurVS", L"shaders/blur.hlsl", nullptr, "VS", "vs_5_1"},
		{"AmbientOcclusionBlurPS", L"shaders/blur.hlsl", nullptr, "PS", "ps_5_1"},

		{"SkyVS", L"shaders/sky.hlsl", nullptr, "VS", "vs_5_1"},
		{"SkyPS", L"shaders/sky.hlsl", nullptr, "PS", "ps_5_1"},
	};

	for (const auto& [name, path, defines, EntryPoint, target] : shaders)
	{
		mShaders[name] = Utils::CompileShader(PREFIX(path), defines, EntryPoint, target);
	}

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void ApplicationInstance::BuildSceneGeometry()
{
	GeometryGenerator generator;

	using NamedMesh = std::pair<const std::string, GeometryGenerator::MeshData>;

	std::array<NamedMesh, 5> meshes =
	{
		NamedMesh("box",      generator.CreateBox(1.0f, 1.0f, 1.0f, 3)),
		NamedMesh("grid",     generator.CreateGrid(20.0f, 30.0f, 60, 40)),
		NamedMesh("sphere",   generator.CreateSphere(0.5f, 20, 20)),
		NamedMesh("cylinder", generator.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20)),
		NamedMesh("quad",     generator.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f))
	};

	const auto lambda = [](UINT sum, const NamedMesh& mesh)
	{
		return sum + mesh.second.vertices.size();
	};

	const UINT TotalVertexCount = std::accumulate(meshes.begin(), meshes.end(), 0, lambda);
	std::vector<Vertex> vertices(TotalVertexCount);

	for (auto iter = vertices.begin(); const auto& [name, mesh] : meshes)
	{
		for (const GeometryGenerator::VertexData& vertex : mesh.vertices)
		{
			iter->position = vertex.position;
			iter->normal = vertex.normal;
			iter->TexCoord = vertex.TexCoord;
			iter->tangent = vertex.tangent;

			iter++;
		}
	}

	std::vector<uint16_t> indices;

	for (auto& [name, mesh] : meshes)
	{
		indices.insert(indices.end(), mesh.GetIndices16().begin(), mesh.GetIndices16().end());
	}

	const UINT VertexBufferByteSize = vertices.size() * sizeof(Vertex);
	const UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

	auto geometry = std::make_unique<MeshGeometry>();
	geometry->name = "meshes";

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
	geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
	geometry->IndexBufferByteSize = IndexBufferByteSize;

	UINT SubMeshVertexOffset = 0;
	UINT SubMeshIndexOffset = 0;

	for (auto i = meshes.begin(); i != meshes.end(); ++i)
	{
		if (i != meshes.begin())
		{
			const GeometryGenerator::MeshData& PrevMesh = std::prev(i)->second;

			SubMeshVertexOffset += PrevMesh.vertices.size();
			SubMeshIndexOffset += PrevMesh.indices32.size();
		}

		const auto& [name, mesh] = *i;

		SubMeshGeometry SubMesh;
		SubMesh.IndexCount = mesh.indices32.size();
		SubMesh.StartIndexLocation = SubMeshIndexOffset;
		SubMesh.BaseVertexLocation = SubMeshVertexOffset;

		geometry->DrawArgs[name] = SubMesh;
	}

	mMeshGeometries[geometry->name] = std::move(geometry);
}

void ApplicationInstance::BuildSkullGeometry()
{
	std::ifstream stream(PREFIX(L"../models/skull.txt"));
	//std::ifstream stream("../../../models/skull.txt");

	if (!stream)
	{
		MessageBox(0, L"models/skull.txt not found", 0, 0);
		return;
	}

	UINT VertexCount = 0;
	UINT TriangleCount = 0;
	std::string ignore;

	stream >> ignore >> VertexCount;
	stream >> ignore >> TriangleCount;
	stream >> ignore >> ignore >> ignore >> ignore;

	const XMFLOAT3 vMinf3(+MathHelper::infinity, +MathHelper::infinity, +MathHelper::infinity);
	const XMFLOAT3 vMaxf3(-MathHelper::infinity, -MathHelper::infinity, -MathHelper::infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	std::vector<Vertex> vertices(VertexCount);

	for (UINT i = 0; i < VertexCount; ++i)
	{
		stream >> vertices[i].position.x >> vertices[i].position.y >> vertices[i].position.z;
		stream >> vertices[i].normal.x >> vertices[i].normal.y >> vertices[i].normal.z;

		const XMVECTOR P = XMLoadFloat3(&vertices[i].position);

		// project point onto unit sphere and generate spherical texture coordinates
		XMFLOAT3 spherePos;
		XMStoreFloat3(&spherePos, XMVector3Normalize(P));

		float theta = atan2f(spherePos.z, spherePos.x);

		// put in [0, 2pi]
		if (theta < 0.0f)
		{
			theta += XM_2PI;
		}

		const float phi = acosf(spherePos.y);

		const float u = theta / (2.0f * XM_PI);
		const float v = phi / XM_PI;

		vertices[i].TexCoord = { u, v };

		const XMVECTOR N = XMLoadFloat3(&vertices[i].normal);

		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		if (std::fabsf(XMVectorGetX(XMVector3Dot(N, up))) < 1.0f - 0.001f)
		{
			const XMVECTOR T = XMVector3Normalize(XMVector3Cross(up, N));
			XMStoreFloat3(&vertices[i].tangent, T);
		}
		else
		{
			up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			const XMVECTOR T = XMVector3Normalize(XMVector3Cross(N, up));
			XMStoreFloat3(&vertices[i].tangent, T);
		}

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

	stream >> ignore;
	stream >> ignore;
	stream >> ignore;

	std::vector<std::int32_t> indices(3 * TriangleCount);

	for (UINT i = 0; i < TriangleCount; ++i)
	{
		stream >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	stream.close();

	const UINT VertexBufferByteSize = vertices.size() * sizeof(Vertex);
	const UINT IndexBufferByteSize = indices.size() * sizeof(uint32_t);

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

	SubMeshGeometry SubMesh;
	SubMesh.IndexCount = indices.size();
	SubMesh.StartIndexLocation = 0;
	SubMesh.BaseVertexLocation = 0;
	SubMesh.BoundingBox = bounds;

	geometry->DrawArgs[geometry->name] = SubMesh;

	mMeshGeometries[geometry->name] = std::move(geometry);
}

void ApplicationInstance::BuildPipelineStateObjects()
{
	std::map<std::string, D3D12_GRAPHICS_PIPELINE_STATE_DESC> descs;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC base;
	ZeroMemory(&base, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	base.pRootSignature = mRootSignature.Get();
	base.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["VS"]->GetBufferPointer());
	base.VS.BytecodeLength = mShaders["VS"]->GetBufferSize();
	base.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["PS"]->GetBufferPointer());
	base.PS.BytecodeLength = mShaders["PS"]->GetBufferSize();
	base.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	base.SampleMask = UINT_MAX;
	base.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	base.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	base.InputLayout.pInputElementDescs = mInputLayout.data();
	base.InputLayout.NumElements = mInputLayout.size();
	base.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	base.NumRenderTargets = 1;
	base.RTVFormats[0] = mBackBufferFormat;
	base.DSVFormat = mDepthStencilFormat;
	base.SampleDesc.Count = m4xMSAAState ? 4 : 1;
	base.SampleDesc.Quality = m4xMSAAState ? (m4xMSAAQuality - 1) : 0;

	// opaque
	{
		const std::string name = "opaque";
		descs[name] = base;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs[name];
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects[name])));
	}

	// opaque normal mapping
	{
		const std::string name = "opaque_normal_mapping";
		descs[name] = descs["opaque"];

		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs[name];
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["NormalMappingPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["NormalMappingPS"]->GetBufferSize();

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects[name])));
	}

	// opaque wireframe
	{
		descs["opaque_wireframe"] = base;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["opaque_wireframe"];

		desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["opaque_wireframe"])));
	}

	// shadow
	{
		descs["shadow"] = base;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["shadow"];

		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["ShadowVS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["ShadowVS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["ShadowPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["ShadowPS"]->GetBufferSize();

		desc.RasterizerState.DepthBias = 100000;
		desc.RasterizerState.DepthBiasClamp = 0.0f;
		desc.RasterizerState.SlopeScaledDepthBias = 1.0f;

		// shadow pass does not have a render target
		desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		desc.NumRenderTargets = 0;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["shadow"])));
	}

	// normals
	{
		descs["normals"] = base;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["normals"];

		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["NormalsVS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["NormalsVS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["NormalsPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["NormalsPS"]->GetBufferSize();

		desc.RTVFormats[0] = SSAO::NormalMapFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.DSVFormat = mDepthStencilFormat;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["normals"])));
	}

	// ambient occlusion
	{
		const std::string name = "AmbientOcclusion";

		descs[name] = base;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs[name];

		desc.InputLayout = { nullptr, 0 };
		desc.pRootSignature = mAmbientOcclusionRootSignature.Get();

		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["AmbientOcclusionVS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["AmbientOcclusionVS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["AmbientOcclusionPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["AmbientOcclusionPS"]->GetBufferSize();

		// SSAO doesn't need the depth buffer
		desc.DepthStencilState.DepthEnable = false;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.RTVFormats[0] = SSAO::AmbientMapFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects[name])));
	}

	// ambient occlusion blur
	{
		const std::string name = "AmbientOcclusionBlur";

		descs[name] = descs["AmbientOcclusion"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs[name];

		desc.InputLayout = { nullptr, 0 };
		desc.pRootSignature = mAmbientOcclusionRootSignature.Get();

		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["AmbientOcclusionBlurVS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["AmbientOcclusionBlurVS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["AmbientOcclusionBlurPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["AmbientOcclusionBlurPS"]->GetBufferSize();

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects[name])));
	}

	// debug
	{
		descs["debug"] = base;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["debug"];

		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["DebugVS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["DebugVS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["DebugPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["DebugPS"]->GetBufferSize();

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["debug"])));
	}

	// sky
	{
		descs["sky"] = base;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["sky"];

		desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["SkyVS"]->GetBufferPointer());
		desc.VS.BytecodeLength = mShaders["SkyVS"]->GetBufferSize();
		desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mShaders["SkyPS"]->GetBufferPointer());
		desc.PS.BytecodeLength = mShaders["SkyPS"]->GetBufferSize();

		// the camera is inside the sky sphere
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		// we normalized depth values at z = 1 and the depth buffer was cleared to 1 
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["sky"])));
	}

	// sky wireframe
	{
		descs["sky_wireframe"] = descs["sky"];
		D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc = descs["sky_wireframe"];

		desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["sky_wireframe"])));
	}
}

void ApplicationInstance::BuildFrameResources()
{
	for (int i = 0; i < kFrameResourcesCount; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(),
																  2, // +1 shadow
																  mRenderItems.size(),
																  mMaterials.size()));
	}
}

void ApplicationInstance::BuildMaterials()
{
	using MaterialInfo = std::tuple<const std::string,	// name
									const int,			// diffuse texture index
									const int,			// normal texture index
									const XMFLOAT4,		// diffuse
									const XMFLOAT3,		// fresnel
									const float>;		// roughness

	const std::array<MaterialInfo, 5> materials =
	{
		MaterialInfo("bricks", 0, 1, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.10f, 0.10f, 0.10f), 0.3f),
		MaterialInfo("tile",   2, 3, XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f), XMFLOAT3(0.20f, 0.20f, 0.20f), 0.1f),
		MaterialInfo("mirror", 4, 5, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(0.98f, 0.97f, 0.95f), 0.1f),
		MaterialInfo("skull",  4, 5, XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f), XMFLOAT3(0.60f, 0.60f, 0.60f), 0.2f),
		MaterialInfo("sky",    6, 7, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.10f, 0.10f, 0.10f), 1.0f)
	};

	UINT MaterialBufferIndex = 0;

	for (const auto& [name, DiffuseTextureIndex, NormalTextureIndex, diffuse, fresnel, roughness] : materials)
	{
		auto material = std::make_unique<Material>();
		material->name = name;
		material->ConstantBufferIndex = MaterialBufferIndex++;
		material->DiffuseSRVHeapIndex = DiffuseTextureIndex;
		material->NormalSRVHeapIndex = NormalTextureIndex;
		material->DiffuseAlbedo = diffuse;
		material->FresnelR0 = fresnel;
		material->roughness = roughness;

		mMaterials[material->name] = std::move(material);
	}
}

void ApplicationInstance::BuildRenderItems()
{
	UINT ObjectCBIndex = 0;

	using data = std::tuple<const RenderLayer,	// layer
							const std::string,	// mesh
							const std::string,	// buffer
							const std::string,	// material
							const XMMATRIX,		// world matrix
							const XMMATRIX>;	// texcoord transform

	std::vector<data> items =
	{
		{RenderLayer::sky,    "sphere", "meshes", "sky",    XMMatrixScaling(5000.0f, 5000.0f, 5000.0f),
															XMMatrixIdentity()},
		{RenderLayer::debug,  "quad",   "meshes", "bricks", XMMatrixIdentity(),
															XMMatrixIdentity()},
		{RenderLayer::opaque, "box",    "meshes", "bricks", XMMatrixScaling(2.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f),
															XMMatrixScaling(1.0f, 0.5f, 1.0f)},
		{RenderLayer::opaque, "skull",  "skull",  "skull",  XMMatrixScaling(0.4f, 0.4f, 0.4f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f),
															XMMatrixIdentity()},
		{RenderLayer::opaque, "grid",   "meshes", "tile",   XMMatrixIdentity(),
															XMMatrixScaling(8.0f, 8.0f, 1.0f)}
	};

	for (UINT i = 0; i < 5; ++i)
	{
		for (UINT j = 0; j < 2; ++j)
		{
			const int side = j * 2 - 1;

			// cylinder
			{
				const XMMATRIX world = XMMatrixTranslation(side * 5.0f, 1.5f, -10.0f + i * 5.0f);
				const data item(RenderLayer::opaque, "cylinder", "meshes", "bricks", world, XMMatrixScaling(1.5f, 2.0f, 1.0f));
				items.push_back(item);
			}

			// sphere
			{
				const XMMATRIX world = XMMatrixTranslation(side * 5.0f, 3.5f, -10.0f + i * 5.0f);
				const data item(RenderLayer::opaque, "sphere", "meshes", "mirror", world, XMMatrixIdentity());
				items.push_back(item);
			}
		}
	}

	for (const auto& [layer, mesh, buffer, material, world, TexCoordTransform] : items)
	{
		auto item = std::make_unique<RenderItem>();

		XMStoreFloat4x4(&item->world, world);
		XMStoreFloat4x4(&item->TexCoordTransform, TexCoordTransform);
		item->ConstantBufferIndex = ObjectCBIndex++;
		item->geometry = mMeshGeometries[buffer].get();
		item->material = mMaterials[material].get();
		item->PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		item->IndexCount = item->geometry->DrawArgs[mesh].IndexCount;
		item->StartIndexLocation = item->geometry->DrawArgs[mesh].StartIndexLocation;
		item->BaseVertexLocation = item->geometry->DrawArgs[mesh].BaseVertexLocation;

		mLayerRenderItems[static_cast<int>(layer)].push_back(item.get());

		mRenderItems.push_back(std::move(item));
	}
}

void ApplicationInstance::DrawRenderItems(ID3D12GraphicsCommandList* CommandList, const std::vector<RenderItem*>& RenderItems)
{
	const UINT ObjectCBByteSize = Utils::GetConstantBufferByteSize(sizeof(ObjectConstants));
	const auto ObjectCB = mCurrentFrameResource->ObjectCB->GetResource();

	for (const auto& item : RenderItems)
	{
		//if (!item->bIsVisible)
		//{
		//	continue;
		//}

		const D3D12_VERTEX_BUFFER_VIEW& VertexBufferView = item->geometry->GetVertexBufferView();
		CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
		const D3D12_INDEX_BUFFER_VIEW& IndexBufferView = item->geometry->GetIndexBufferView();
		CommandList->IASetIndexBuffer(&IndexBufferView);

		CommandList->IASetPrimitiveTopology(item->PrimitiveTopology);

		// bind object constant buffer
		const D3D12_GPU_VIRTUAL_ADDRESS ObjectCBAddress = ObjectCB->GetGPUVirtualAddress() + item->ConstantBufferIndex * ObjectCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(0, ObjectCBAddress);

		CommandList->DrawIndexedInstanced(item->IndexCount, 1, item->StartIndexLocation, item->BaseVertexLocation, 0);
	}
}

void ApplicationInstance::DrawSceneToShadowMap()
{
	mCommandList->RSSetViewports(1, &mShadowMap->GetViewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->GetScissorRect());

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->GetResource(),
																				   D3D12_RESOURCE_STATE_GENERIC_READ,
																				   D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mCommandList->ResourceBarrier(1, &transition);
	}

	// clear shadow map
	mCommandList->ClearDepthStencilView(mShadowMap->GetDSV(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	// set null render target
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE& dsv = mShadowMap->GetDSV();
		mCommandList->OMSetRenderTargets(0, nullptr, true, &dsv);
	}

	// bind main pass constant buffer
	auto MainPassCB = mCurrentFrameResource->MainPassCB->GetResource();
	const UINT MainPassCBByteSize = Utils::GetConstantBufferByteSize(sizeof(MainPassConstants));
	const D3D12_GPU_VIRTUAL_ADDRESS address = MainPassCB->GetGPUVirtualAddress() + 1 * MainPassCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(1, address);

	mCommandList->SetPipelineState(mPipelineStateObjects["shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::opaque)]);
	
	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->GetResource(),
																				   D3D12_RESOURCE_STATE_DEPTH_WRITE,
																				   D3D12_RESOURCE_STATE_GENERIC_READ);
		mCommandList->ResourceBarrier(1, &transition);
	}
}

void ApplicationInstance::DrawNormalsAndDepth()
{
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto NormalMap = mSSAO->GetNormalMap();
	auto NormalMapRTV = mSSAO->GetNormalMapRTV();

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(NormalMap,
																				   D3D12_RESOURCE_STATE_GENERIC_READ,
																				   D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	const FLOAT ClearValue[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	mCommandList->ClearRenderTargetView(NormalMapRTV, ClearValue, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &NormalMapRTV, true, &dsv);
	}

	// bind main pass constant buffer
	auto MainPassCB = mCurrentFrameResource->MainPassCB->GetResource();
	mCommandList->SetGraphicsRootConstantBufferView(1, MainPassCB->GetGPUVirtualAddress());

	// draw scene normals
	mCommandList->SetPipelineState(mPipelineStateObjects["normals"].Get());
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::opaque)]);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(NormalMap,
																				   D3D12_RESOURCE_STATE_RENDER_TARGET,
																				   D3D12_RESOURCE_STATE_GENERIC_READ);
		mCommandList->ResourceBarrier(1, &transition);
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ApplicationInstance::GetCpuSRV(const int index) const
{
	auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCBVSRVUAVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCBVSRVUAVDescriptorSize);
	return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ApplicationInstance::GetGpuSRV(const int index) const
{
	auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	srv.Offset(index, mCBVSRVUAVDescriptorSize);
	return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ApplicationInstance::GetDSV(const int index) const
{
	auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDSVHeap->GetCPUDescriptorHandleForHeapStart());
	dsv.Offset(index, mDSVDescriptorSize);
	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ApplicationInstance::GetRTV(const int index) const
{
	auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVHeap->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(index, mRTVDescriptorSize);
	return rtv;
}

const samplers& ApplicationInstance::GetStaticSamplers()
{
	static const samplers StaticSamplers = []() -> samplers
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

		const CD3DX12_STATIC_SAMPLER_DESC shadow
		(
			6,
			D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			0.0f,
			16,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
		);

		return
		{
			PointWrap,
			PointClamp,
			LinearWrap,
			LinearClamp,
			AnisotropicWrap,
			AnisotropicClamp,
			shadow
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