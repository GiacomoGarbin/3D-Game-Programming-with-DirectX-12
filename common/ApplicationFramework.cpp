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

    glfwDestroyWindow(mMainWindow.Get());
    glfwTerminate();
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
    if (glfwInit() == GLFW_FALSE)
    {
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mMainWindow = glfwCreateWindow(mMainWindowWidth, mMainWindowHeight, mMainWindowTitle.c_str(), nullptr, nullptr);

    if (!mMainWindow) return false;

    glfwSetWindowUserPointer(mMainWindow.Get(), this);

    glfwSetWindowSizeCallback(mMainWindow.Get(), [](GLFWwindow* window, int width, int height)
    {
        ApplicationFramework* app = reinterpret_cast<ApplicationFramework*>(glfwGetWindowUserPointer(window));
        app->OnResize(window, width, height);
    });

    glfwSetMouseButtonCallback(mMainWindow.Get(), [](GLFWwindow* window, int button, int action, int mods)
    {
        ApplicationFramework* app = reinterpret_cast<ApplicationFramework*>(glfwGetWindowUserPointer(window));
        app->OnMouseButton(window, button, action, mods);
    });

    glfwSetCursorPosCallback(mMainWindow.Get(), [](GLFWwindow* window, double xpos, double ypos)
    {
        ApplicationFramework* app = reinterpret_cast<ApplicationFramework*>(glfwGetWindowUserPointer(window));
        app->OnMouseMove(window, xpos, ypos);
    });

    glfwSetKeyCallback(mMainWindow.Get(), [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        ApplicationFramework* app = reinterpret_cast<ApplicationFramework*>(glfwGetWindowUserPointer(window));
        app->OnKeyButton(window, key, scancode, action, mods);
    });

    return true;
}

bool ApplicationFramework::InitDirect3D()
{
    // enable D3D12 debug layer
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> controller;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&controller)));
        controller->EnableDebugLayer();
    }

    ThrowIfFailed(CreateDXGIFactory(IID_PPV_ARGS(&mFactory)));
    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice)));
    ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

    mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDSVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCBVSRVUAVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // check 4x MSAA support
    {

    }

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
    mSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC desc;
    desc.BufferDesc.Width = mMainWindowWidth;
    desc.BufferDesc.Height = mMainWindowHeight;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferDesc.Format = mBackBufferFormat;
    desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.SampleDesc.Count = m4xMSAAState ? 4 : 1;
    desc.SampleDesc.Quality = m4xMSAAState ? (m4xMSAAQuality - 1) : 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = SwapChainBufferSize;
    desc.OutputWindow = glfwGetWin32Window(mMainWindow.Get());
    desc.Windowed = true;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ThrowIfFailed(mFactory->CreateSwapChain(mCommandQueue.Get(), &desc, mSwapChain.GetAddressOf()));
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

    while (!glfwWindowShouldClose(mMainWindow.Get()))
    {
        glfwPollEvents();

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
    mCurrentFence++;

    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

    if (mFence->GetCompletedValue() < mCurrentFence)
    {
        HANDLE handle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, handle));

        WaitForSingleObject(handle, INFINITE);

        CloseHandle(handle);
    }
}
