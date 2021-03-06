#include "ApplicationFramework.h"

class ApplicationInstance : public ApplicationFramework
{
	bool mShowDemoWindow = true;
	XMFLOAT4 mRenderTargetClearColor = { 1, 1, 0, 1 };
	//XMVECTORF32 mRenderTargetClearColor = DirectX::Colors::Yellow;

	virtual void OnResize() override;
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;

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
	return ApplicationFramework::init();
}

void ApplicationInstance::OnResize()
{
	ApplicationFramework::OnResize();
}

void ApplicationInstance::update(GameTimer& timer)
{}

void ApplicationInstance::draw(GameTimer& timer)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	//ImGui::ShowDemoWindow(&mShowDemoWindow);

	{
		ImGui::Begin("clear render target color");
		ImGui::ColorEdit3("clear render target color", &mRenderTargetClearColor.x, 0);
		ImGui::End();
	}

	ImGui::Render();

	ThrowIfFailed(mCommandAllocator->Reset());

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	{
		auto transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(1, &transition);
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), &mRenderTargetClearColor.x, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	auto rtv = GetCurrentBackBufferView();
	auto dsv = GetDepthStencilView();
	mCommandList->OMSetRenderTargets(1, &rtv, true, &dsv);

	ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	{
		auto transition = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		mCommandList->ResourceBarrier(1, &transition);
	}

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* CommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrentBackBufferIndex = (mCurrentBackBufferIndex + 1) % SwapChainBufferSize;

	FlushCommandQueue();
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