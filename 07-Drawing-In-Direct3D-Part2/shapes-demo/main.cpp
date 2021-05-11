#include "ApplicationFramework.h"

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};

struct ConstantBuffer
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class ApplicationInstance : public ApplicationFramework
{
	XMFLOAT4 mRenderTargetClearColor = { 1, 1, 0, 1 };
	//XMVECTORF32 mRenderTargetClearColor = DirectX::Colors::Yellow;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCBVHeap = nullptr;

	//Microsoft::WRL::ComPtr<UploadBuffer<ConstantBuffer>> mConstantBuffer = nullptr;
	std::unique_ptr<UploadBuffer<ConstantBuffer>> mConstantBuffer = nullptr;

	//Microsoft::WRL::ComPtr<MeshGeometry> mBox = nullptr;
	std::unique_ptr<MeshGeometry> mBox = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> mVSByteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> mPSByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineStateObject = nullptr;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	POINT mLastMousePosition;

	virtual void OnResize() override;
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;

	virtual void OnMouseDown(WPARAM state, int x, int y) override;
	virtual void OnMouseUp(WPARAM state, int x, int y) override;
	virtual void OnMouseMove(WPARAM state, int x, int y) override;

	void BuildDescriptorHeap();
	void BuildConstantBuffer();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildMeshGeometry();
	void BuildPipelineStateObject();

public:
	ApplicationInstance(HINSTANCE instance);
	~ApplicationInstance();

	virtual bool init() override;
};

ApplicationInstance::ApplicationInstance(HINSTANCE instance)
	: ApplicationFramework(instance)
{}

ApplicationInstance::~ApplicationInstance()
{}

bool ApplicationInstance::init()
{
	if (!ApplicationFramework::init())
	{
		return false;
	}

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	BuildDescriptorHeap();
	BuildConstantBuffer();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildMeshGeometry();
	BuildPipelineStateObject();

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
	float x = mRadius * std::sin(mPhi) * std::cos(mTheta);
	float z = mRadius * std::sin(mPhi) * std::sin(mTheta);
	float y = mRadius * std::cos(mPhi);

	XMVECTOR eye = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX WorldViewProj = world * view * proj;

	ConstantBuffer buffer;
	XMStoreFloat4x4(&buffer.WorldViewProj, XMMatrixTranspose(WorldViewProj)); // <--
	mConstantBuffer->CopyData(0, buffer);
}

void ApplicationInstance::draw(GameTimer& timer)
{
	//ImGui_ImplDX12_NewFrame();
	//ImGui_ImplWin32_NewFrame();
	//ImGui::NewFrame();

	//{
	//	ImGui::Begin("clear render target color");
	//	ImGui::ColorEdit3("clear render target color", &mRenderTargetClearColor.x, 0);
	//	ImGui::End();
	//}

	//ImGui::Render();

	ThrowIfFailed(mCommandAllocator->Reset());

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPipelineStateObject.Get()));

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

	//ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get() };
	//mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	//ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = mBox->GetVertexBufferView();
	mCommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = mBox->GetIndexBufferView();
	mCommandList->IASetIndexBuffer(&IndexBufferView);
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->SetGraphicsRootDescriptorTable(0, mCBVHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(mBox->DrawArgs["box"].IndexCount, 1, 0, 0, 0);

	{
		CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCommandList->ResourceBarrier(1, &transition);
	}

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* CommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrentBackBufferIndex = (mCurrentBackBufferIndex + 1) % SwapChainBufferSize;

	FlushCommandQueue();
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
		float dx = 0.005f * static_cast<float>(x - mLastMousePosition.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePosition.y);

		mRadius += dx - dy;

		//mRadius = MathHelper::clamp(mRadius, 3.0f, 15.0f);
		mRadius = clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePosition.x = x;
	mLastMousePosition.y = y;
}

void ApplicationInstance::BuildDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;

	ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mCBVHeap.GetAddressOf())));
}

void ApplicationInstance::BuildConstantBuffer()
{
	//mConstantBuffer = Microsoft::WRL::ComPtr<UploadBuffer<ConstantBuffer>>(mDevice.Get(), 1, true /* IsConstantBuffer */);
	mConstantBuffer = std::make_unique<UploadBuffer<ConstantBuffer>>(mDevice.Get(), 1, true /* IsConstantBuffer */);

	UINT size = Utils::GetConstantBufferByteSize(sizeof(ConstantBuffer));

	D3D12_GPU_VIRTUAL_ADDRESS address = mConstantBuffer->GetResource()->GetGPUVirtualAddress();

	int index = 0;
	address += index * size;

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = address;
	desc.SizeInBytes = size;

	mDevice->CreateConstantBufferView(&desc, mCBVHeap->GetCPUDescriptorHandleForHeapStart());
}

void ApplicationInstance::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER params[1];

	CD3DX12_DESCRIPTOR_RANGE table;
	table.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	params[0].InitAsDescriptorTable(1, &table);

	CD3DX12_ROOT_SIGNATURE_DESC desc(1,
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
											   IID_PPV_ARGS(&mRootSignature)));
}

void ApplicationInstance::BuildShadersAndInputLayout()
{
	mVSByteCode = Utils::CompileShader(L"shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mPSByteCode = Utils::CompileShader(L"shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void ApplicationInstance::BuildMeshGeometry()
{
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,
		// back face
		4, 6, 5,
		4, 7, 6,
		// left face
		4, 5, 1,
		4, 1, 0,
		// right face
		3, 2, 6,
		3, 6, 7,
		// top face
		1, 5, 6,
		1, 6, 2,
		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	const UINT VertexBufferByteSize = vertices.size() * sizeof(Vertex);
	const UINT IndexBufferByteSize = indices.size() * sizeof(uint16_t);

	//mBox = Microsoft::WRL::ComPtr<MeshGeometry>();
	mBox = std::make_unique<MeshGeometry>();
	mBox->name = "box";

	ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &mBox->VertexBufferCPU));
	CopyMemory(mBox->VertexBufferCPU->GetBufferPointer(), vertices.data(), VertexBufferByteSize);

	ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &mBox->IndexBufferCPU));
	CopyMemory(mBox->IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);

	mBox->VertexBufferGPU = Utils::CreateDefaultBuffer(mDevice.Get(),
													   mCommandList.Get(),
													   vertices.data(),
													   VertexBufferByteSize,
													   mBox->VertexBufferUploader);

	mBox->IndexBufferGPU = Utils::CreateDefaultBuffer(mDevice.Get(),
													  mCommandList.Get(),
													  indices.data(),
													  IndexBufferByteSize,
													  mBox->IndexBufferUploader);

	mBox->VertexByteStride = sizeof(Vertex);
	mBox->VertexBufferByteSize = VertexBufferByteSize;

	mBox->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBox->IndexBufferByteSize = IndexBufferByteSize;

	SubMeshGeometry SubMesh;
	SubMesh.IndexCount = indices.size();
	SubMesh.StartIndexLocation = 0;
	SubMesh.BaseIndexVertexLocation = 0;

	mBox->DrawArgs["box"] = SubMesh;
}

void ApplicationInstance::BuildPipelineStateObject()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	desc.pRootSignature = mRootSignature.Get();
	desc.VS.pShaderBytecode = reinterpret_cast<BYTE*>(mVSByteCode->GetBufferPointer());
	desc.VS.BytecodeLength = mVSByteCode->GetBufferSize();
	desc.PS.pShaderBytecode = reinterpret_cast<BYTE*>(mPSByteCode->GetBufferPointer());
	desc.PS.BytecodeLength = mPSByteCode->GetBufferSize();
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
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&mPipelineStateObject)));
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