#include "ApplicationFramework.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "camera.h"

#include <numeric>
#include <sstream>
#include <fstream>

#define RENDERDOC_BUILD 0

const UINT kFrameResourcesCount = 3;

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

	MainPassConstants mMainPassCB;

	Camera mCamera;
	//BoundingFrustum mCameraFrustum;
	//bool mIsFrustumCullingEnabled = true;

	bool mIsWireFrameEnabled = false;

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
	void UpdateMainPassCB(const GameTimer& timer);

	void BuildRootSignatures();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildSceneGeometry();
	void BuildSkullGeometry();
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

	mCamera.LookAt(XMFLOAT3(5.0f, 4.0f, -15.0f),
				   XMFLOAT3(0.0f, 1.0f, 0.0f),
				   XMFLOAT3(0.0f, 1.0f, 0.0f));

	LoadTextures();
	BuildRootSignatures();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildSceneGeometry();
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

	mCamera.SetLens(0.25f * XM_PI, GetAspectRatio(), 1.0f, 1000.0f);
	//BoundingFrustum::CreateFromMatrix(mCameraFrustum, mCamera.GetProj());
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

	AnimateMaterials(timer);
	UpdateObjectCBs(timer);
	UpdateMaterialBuffer(timer);
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

	ThrowIfFailed(mCommandList->Reset(CommandAllocator.Get(),
									  mPipelineStateObjects[mIsWireFrameEnabled ? "opaque_wireframe" : "opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
																				   D3D12_RESOURCE_STATE_PRESENT,
																				   D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDepthStencilView();
		mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);
	}

	{
		ID3D12DescriptorHeap* heaps[] = { mCBVSRVUAVDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	}

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// bind main pass constant buffer
	auto MainPassCB = mCurrentFrameResource->MainPassCB->GetResource();
	mCommandList->SetGraphicsRootConstantBufferView(1, MainPassCB->GetGPUVirtualAddress());

	// bind all materials
	auto MaterialBuffer = mCurrentFrameResource->MaterialBuffer->GetResource();
	mCommandList->SetGraphicsRootShaderResourceView(2, MaterialBuffer->GetGPUVirtualAddress());
	
	// bind sky cube map texture
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE SkyTextureDescriptor(mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		SkyTextureDescriptor.Offset(mSkyTextureHeapIndex, mCBVSRVUAVDescriptorSize);
		mCommandList->SetGraphicsRootDescriptorTable(3, SkyTextureDescriptor);
	}

	// bind all other textures
	mCommandList->SetGraphicsRootDescriptorTable(4, mCBVSRVUAVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// draw scene
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::opaque)]);

	// draw sky
	mCommandList->SetPipelineState(mPipelineStateObjects[mIsWireFrameEnabled ? "sky_wireframe" : "sky"].Get());
	DrawRenderItems(mCommandList.Get(), mLayerRenderItems[static_cast<int>(RenderLayer::sky)]);

	// imgui
	{
		ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	}

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

	//XMVECTOR determinant = XMMatrixDeterminant(view);
	//const XMMATRIX ViewInverse = XMMatrixInverse(&determinant, view);

	//auto CurrentInstanceBuffer = mCurrentFrameResource->InstanceBuffer.get();

	//for (auto& object : mRenderItems)
	//{
	//	UINT VisibleInstanceCount = 0;

	//	for (const InstanceData& instance : object->instances)
	//	{
	//		const XMMATRIX world = XMLoadFloat4x4(&instance.world);
	//		const XMMATRIX TexCoordTransform = XMLoadFloat4x4(&instance.TexCoordTransform);

	//		XMVECTOR determinant = XMMatrixDeterminant(world);
	//		const XMMATRIX WorldInverse = XMMatrixInverse(&determinant, world);

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

			CurrentMaterialBuffer->CopyData(material->ConstantBufferIndex, buffer);

			(material->DirtyFramesCount)--;
		}
	}
}

void ApplicationInstance::UpdateMainPassCB(const GameTimer& timer)
{
	XMVECTOR determinant;

	XMMATRIX view = mCamera.GetView();
	determinant = XMMatrixDeterminant(view);
	XMMATRIX ViewInverse = XMMatrixInverse(&determinant, view);
	XMMATRIX proj = mCamera.GetProj();
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
	mMainPassCB.EyePositionWorld = mCamera.GetPositionF();
	mMainPassCB.RenderTargetSize = XMFLOAT2(mMainWindowWidth, mMainWindowHeight);
	mMainPassCB.RenderTargetSizeInverse = XMFLOAT2(1.0f / mMainWindowWidth, 1.0f / mMainWindowHeight);
	mMainPassCB.NearPlane = 1.0f;
	mMainPassCB.FarPlane = 1000.0f;
	mMainPassCB.DeltaTime = timer.GetDeltaTime();
	mMainPassCB.TotalTime = timer.GetTotalTime();

	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	mMainPassCB.lights[0].direction = { +0.57735f, -0.57735f, +0.57735f };
	mMainPassCB.lights[0].strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.lights[1].direction = { -0.57735f, -0.57735f, +0.57735f };
	mMainPassCB.lights[1].strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.lights[2].direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.lights[2].strength = { 0.15f, 0.15f, 0.15f };

	auto CurrentMainPassCB = mCurrentFrameResource->MainPassCB.get();
	CurrentMainPassCB->CopyData(0, mMainPassCB);
}

void ApplicationInstance::LoadTextures()
{
	const auto LoadTexture = [this](const std::string& name)
	{
		// very bad way to convert a narrow string to a wide string
		const std::wstring wname(name.begin(), name.end());

		auto texture = std::make_unique<Texture>();
		texture->name = name;
#if RENDERDOC_BUILD
		texture->filename = L"../../../textures/" + wname + L".dds";
#else // RENDERDOC_BUILD
		texture->filename = L"../textures/" + wname + L".dds";
#endif // RENDERDOC_BUILD

		ThrowIfFailed(CreateDDSTextureFromFile12(mDevice.Get(),
												 mCommandList.Get(),
												 texture->filename.c_str(),
												 texture->resource,
												 texture->UploadHeap));

		mTextures[texture->name] = std::move(texture);
	};

	const std::array<const std::string, 4> textures =
	{
		"bricks2",
		"tile",
		"white1x1",
		"grasscube1024"
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
	// sky cube map texture (t0, space0)
	{
		CD3DX12_DESCRIPTOR_RANGE TextureTable;
		TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		params[3].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	}
	// all other textures (t1, space0)
	{
		CD3DX12_DESCRIPTOR_RANGE TextureTable;
		TextureTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, mTextures.size() - 1, 1, 0);
		params[4].InitAsDescriptorTable(1, &TextureTable, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	auto StaticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC desc(sizeof(params) / sizeof(CD3DX12_ROOT_PARAMETER),
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
	// create SRV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = mTextures.size();
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mCBVSRVUAVDescriptorHeap.GetAddressOf())));
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE descriptor(mCBVSRVUAVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto CreateSRV = [&](const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
						 const D3D12_SRV_DIMENSION dimension = D3D12_SRV_DIMENSION_TEXTURE2D)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = texture->GetDesc().Format;
		desc.ViewDimension = dimension;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		switch (desc.ViewDimension)
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
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
				desc.TextureCube.MostDetailedMip = 0;
				desc.TextureCube.MipLevels = texture->GetDesc().MipLevels;
				desc.TextureCube.ResourceMinLODClamp = 0.0f;
				break;
		}

		mDevice->CreateShaderResourceView(texture.Get(), &desc, descriptor);

		descriptor.Offset(1, mCBVSRVUAVDescriptorSize);
	};

	for (const auto& [name, texture] : mTextures)
	{
		if (name == "grasscube1024")
		{
			CreateSRV(texture->resource, D3D12_SRV_DIMENSION_TEXTURECUBE);
			mSkyTextureHeapIndex = 3;
		}
		else
		{
			CreateSRV(texture->resource);
		}
	}
}

void ApplicationInstance::BuildShadersAndInputLayout()
{
#if RENDERDOC_BUILD
	mShaders["VS"] = Utils::CompileShader(L"../../shaders/default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["PS"] = Utils::CompileShader(L"../../shaders/default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["SkyVS"] = Utils::CompileShader(L"../../shaders/sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["SkyPS"] = Utils::CompileShader(L"../../shaders/sky.hlsl", nullptr, "PS", "ps_5_1");
#else // RENDERDOC_BUILD
	mShaders["VS"] = Utils::CompileShader(L"shaders/default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["PS"] = Utils::CompileShader(L"shaders/default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["SkyVS"] = Utils::CompileShader(L"shaders/sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["SkyPS"] = Utils::CompileShader(L"shaders/sky.hlsl", nullptr, "PS", "ps_5_1");
#endif // RENDERDOC_BUILD

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void ApplicationInstance::BuildSceneGeometry()
{
	GeometryGenerator generator;

	using NamedMesh = std::pair<const std::string, GeometryGenerator::MeshData>;

	std::array<NamedMesh, 4> meshes =
	{
		NamedMesh("box", generator.CreateBox(1.0f, 1.0f, 1.0f, 3)),
		NamedMesh("grid", generator.CreateGrid(20.0f, 30.0f, 60, 40)),
		NamedMesh("sphere", generator.CreateSphere(0.5f, 20, 20)),
		NamedMesh("cylinder", generator.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20))
	};

	const auto lambda = [](UINT sum, const NamedMesh& mesh)
	{
		return sum + mesh.second.vertices.size();
	};

	const UINT TotalVertexCount = std::accumulate(meshes.begin(), meshes.end(), 0, lambda);
	std::vector<Vertex> vertices(TotalVertexCount);

	for (auto iter = vertices.begin(); const auto & [name, mesh] : meshes)
	{
		for (const GeometryGenerator::VertexData& vertex : mesh.vertices)
		{
			iter->position = vertex.position;
			iter->normal = vertex.normal;
			iter->TexCoord = vertex.TexCoord;

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
#if RENDERDOC_BUILD
	std::ifstream stream("../../../models/skull.txt");
#else // RENDERDOC_BUILD
	std::ifstream stream("../models/skull.txt");
#endif // RENDERDOC_BUILD

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
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObjects["opaque_wireframe"])));
	}

	// sky
	{
		descs["sky"] = descs["opaque"];
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
																  1,
																  mRenderItems.size(),
																  mMaterials.size()));
	}
}

void ApplicationInstance::BuildMaterials()
{
	// name, DiffuseSRVHeapIndex, diffuse, fresnel, roughness
	using MaterialInfo = const std::tuple<const std::string, const int, const XMFLOAT4, const XMFLOAT3, const float>;

	const std::array<MaterialInfo, 5> materials =
	{
		MaterialInfo("bricks", 0, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f),    0.3f),
		MaterialInfo("tile",   1, XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f), XMFLOAT3(0.2f, 0.2f, 0.2f),    0.1f),
		MaterialInfo("mirror", 2, XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f), XMFLOAT3(0.98f, 0.97f, 0.95f), 0.1f),
		MaterialInfo("skull",  2, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), XMFLOAT3(0.2f, 0.2f, 0.2f),    0.2f),
		MaterialInfo("sky",    3, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.1f, 0.1f, 0.1f),    1.0f)
	};

	UINT MaterialBufferIndex = 0;

	for (const auto& [name, index, diffuse, fresnel, roughness] : materials)
	{
		auto material = std::make_unique<Material>();
		material->name = name;
		material->ConstantBufferIndex = MaterialBufferIndex++;
		material->DiffuseSRVHeapIndex = index;
		material->DiffuseAlbedo = diffuse;
		material->FresnelR0 = fresnel;
		material->roughness = roughness;

		mMaterials[material->name] = std::move(material);
	}
}

void ApplicationInstance::BuildRenderItems()
{
	UINT ObjectCBIndex = 0;

	// layer, mesh, buffer, material, world, texcoordtransform
	using data = std::tuple<const RenderLayer, const std::string, const std::string, const std::string, const XMMATRIX, const XMMATRIX>;

	std::vector<data> items =
	{
		data(RenderLayer::sky,    "sphere", "meshes", "sky",    XMMatrixScaling(5000.0f, 5000.0f, 5000.0f),                                XMMatrixIdentity()),
		data(RenderLayer::opaque, "box",    "meshes", "bricks", XMMatrixScaling(2.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f), XMMatrixIdentity()),
		data(RenderLayer::opaque, "skull",  "skull",  "skull",  XMMatrixScaling(0.4f, 0.4f, 0.4f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f), XMMatrixIdentity()),
		data(RenderLayer::opaque, "grid",   "meshes", "tile",   XMMatrixIdentity(),                                                        XMMatrixScaling(8.0f, 8.0f, 1.0f))
	};

	for (UINT i = 0; i < 5; ++i)
	{
		for (UINT j = 0; j < 2; ++j)
		{
			const int side = j * 2 - 1;

			// cylinder
			{
				const XMMATRIX world = XMMatrixTranslation(side * 5.0f, 1.5f, -10.0f + i * 5.0f);
				const data item(RenderLayer::opaque, "cylinder", "meshes", "bricks", world, XMMatrixIdentity());
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