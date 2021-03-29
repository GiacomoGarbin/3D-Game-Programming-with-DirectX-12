#include "ApplicationFramework.h"

ApplicationFramework::ApplicationFramework()
{

}

ApplicationFramework::~ApplicationFramework()
{
    if (mDevice)
    {
        FlushCommandQueue();
    }
}

bool ApplicationFramework::init()
{
    if (!InitMainWindow())
    {
        return false;
    }

    if (!InitDirect3D())
    {
        return false;
    }

    OnResize();

    return true;
}

bool ApplicationFramework::InitMainWindow()
{
    return true;
}

bool ApplicationFramework::InitDirect3D()
{
    ThrowIfFailed(CreateDXGIFactory(IID_PPV_ARGS(&mFactory)));

    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));

    ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

    mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDSVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    CreateCommandObjects();
    CreateSwapChain();
    CreateRTVAndDSVDescriptorHeaps();

    return true;
}

void ApplicationFramework::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC desc;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    ThrowIfFailed(mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mCommandQueue)));

    ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCommandAllocator.GetAddressOf())));

    ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf())));

    mCommandList->Close();
}

void ApplicationFramework::CreateSwapChain()
{

}

void ApplicationFramework::CreateRTVAndDSVDescriptorHeaps()
{
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        desc.NumDescriptors = SwapChainBufferSize;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 0;

        ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mRTVHeap.GetAddressOf())));
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        desc.NumDescriptors = 1;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 0;

        ThrowIfFailed(mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDSVHeap.GetAddressOf())));
    }
}

void ApplicationFramework::OnResize()
{

}

bool ApplicationFramework::run()
{
    mGameTimer.reset();

    while (true) // TODO: windows is open
    {
        {
            mGameTimer.tick();

            if (mApplicationPaused)
            {
                // frame stats
                update(mGameTimer);
                draw(mGameTimer);
            }
            else
            {
                Sleep(100);
            }
        }
    }

    return true;
}

ID3D12Resource* ApplicationFramework::GetCurrentBackBuffer() const
{
    return mSwapChainBuffer[mCurrentBackBufferIndex].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE ApplicationFramework::GetCurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVHeap->GetCPUDescriptorHandleForHeapStart(), mCurrentBackBufferIndex, mRTVDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE ApplicationFramework::GetDepthStencilView() const
{
    return mDSVHeap->GetCPUDescriptorHandleForHeapStart();
}

void ApplicationFramework::FlushCommandQueue()
{

}
