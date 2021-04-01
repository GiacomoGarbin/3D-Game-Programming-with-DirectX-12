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

    OnResize(mMainWindow.Get(), mMainWindowWidth, mMainWindowHeight);

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

void ApplicationFramework::OnMouseButton(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        mLastMousePos = XMFLOAT2(x, y);
    }
}

void ApplicationFramework::OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        float dx = XMConvertToRadians(0.25f * (xpos - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * (ypos - mLastMousePos.y));

        //mCamera.pitch(dy);
        //mCamera.rotate(dx);

        mLastMousePos = XMFLOAT2(xpos, ypos);
    }
}

void ApplicationFramework::OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_UNKNOWN) return;

    switch (action)
    {
        case GLFW_PRESS:
            mKeysState[key] = true;
            break;
        case GLFW_RELEASE:
            mKeysState[key] = false;
            break;
        case GLFW_REPEAT:
            break;
    }
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

void ApplicationFramework::OnResize(GLFWwindow* window, int width, int height)
{
    FlushCommandQueue();

    ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

    for (auto& buffer : mSwapChainBuffer)
    {
        buffer.Reset();
    }

    mDepthStencilBuffer.Reset();

    ThrowIfFailed(mSwapChain->ResizeBuffers(SwapChainBufferSize, mMainWindowWidth, mMainWindowHeight, mBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    mCurrentBackBufferIndex = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mRTVHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < SwapChainBufferSize; ++i)
    {
        ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
        mDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, handle);
        handle.Offset(1, mRTVDescriptorSize);
    }

    D3D12_RESOURCE_DESC desc;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = mMainWindowWidth;
    desc.Height = mMainWindowHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc.SampleDesc.Count = m4xMSAAState ? 4 : 1;
    desc.SampleDesc.Quality = m4xMSAAState ? (m4xMSAAQuality - 1) : 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear;
    clear.Format = mDepthStencilFormat;
    clear.DepthStencil.Depth = 1;
    clear.DepthStencil.Stencil = 0;

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(mDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, &clear, IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

    // depth stencil view
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC desc;
        desc.Flags = D3D12_DSV_FLAG_NONE;
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        desc.Format = mDepthStencilFormat;
        desc.Texture2D.MipSlice = 0;

        mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &desc, GetDepthStencilView());

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        mCommandList->ResourceBarrier(1, &barrier);
    }

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* list[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(list), list);

    FlushCommandQueue();

    mScreenViewport.TopLeftX = 0;
    mScreenViewport.TopLeftY = 0;
    mScreenViewport.Width = mMainWindowWidth;
    mScreenViewport.Height = mMainWindowHeight;
    mScreenViewport.MinDepth = 0;
    mScreenViewport.MaxDepth = 1;

    mScissorRect.left = 0;
    mScissorRect.top = 0;
    mScissorRect.right = mMainWindowWidth;
    mScissorRect.bottom = mMainWindowHeight;
}

bool ApplicationFramework::run()
{
    //mGameTimer.reset();

    while (!glfwWindowShouldClose(mMainWindow.Get()))
    {
        glfwPollEvents();

        //mGameTimer.tick();

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
        HANDLE handle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, handle));

        WaitForSingleObject(handle, INFINITE);

        CloseHandle(handle);
    }
}
