#include "ApplicationFramework.h"

class ApplicationInstance : public ApplicationFramework
{
	virtual void OnResize() override;
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;

public:
	ApplicationInstance();
	~ApplicationInstance();

	virtual bool init() override;
};

ApplicationInstance::ApplicationInstance() : ApplicationFramework()
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
	ThrowIfFailed(mCommandAllocator->Reset());

	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), DirectX::Colors::Black, 0, nullptr);
	mCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferView(), true, &GetDepthStencilView());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* CommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrentBackBufferIndex = (mCurrentBackBufferIndex + 1) % SwapChainBufferSize;

	FlushCommandQueue();
}

int main()
{
	ApplicationInstance instance;

	if (!instance.init())
	{
		return 0;
	}

	return instance.run();
}