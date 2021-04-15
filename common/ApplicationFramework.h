#ifndef APPLICATION_FRAMEWORK_H
#define APPLICATION_FRAMEWORK_H

// windows
#include <wrl.h>

// directx
#include "d3dx12.h"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
using namespace DirectX;

// std library
#include <array>

// common
#include "utils.h"
#include "GameTimer.h"

// debug
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

class ApplicationFramework
{
protected:
    ApplicationFramework(HINSTANCE instance);
    ApplicationFramework(const ApplicationFramework& rhs) = delete;
    ApplicationFramework& operator=(const ApplicationFramework& rhs) = delete;
    virtual ~ApplicationFramework();

public:
    static ApplicationFramework* GetApplicationFramework();

    HINSTANCE GetApplicationInstance() const { return mApplicationInstance; }
    HWND GetMainWindow() const { return mMainWindow; }
    float GetAspectRatio() const { return static_cast<float>(mMainWindowWidth) / mMainWindowHeight; }

    bool Get4xMSAAState() const { return m4xMSAAState; }
    void Set4xMSAAState(bool value) { m4xMSAAState = value; }

    int run();
    
    virtual bool init();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateRTVAndDSVDescriptorHeaps();
    virtual void OnResize();
    virtual void update(GameTimer& timer) = 0;
    virtual void draw(GameTimer& timer) = 0;

    virtual void OnMouseDown(WPARAM state, int x, int y);
    virtual void OnMouseUp(WPARAM state, int x, int y);
    virtual void OnMouseMove(WPARAM state, int x, int y);

    bool InitMainWindow();
    bool InitDirect3D();
    void CreateCommandObjects();
    void CreateSwapChain();
    void FlushCommandQueue();

    ID3D12Resource* GetCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

    void CalculateFrameStats();

    static ApplicationFramework* mApplicationFramework;

    HINSTANCE mApplicationInstance = nullptr;
    HWND mMainWindow = nullptr;
    
    bool mApplicationPaused = false;
    bool mWindowMinimized = false;
    bool mWindowMaximized = false;
    bool mWindowResizing = false;
    bool mFullscreenState = false;

    bool m4xMSAAState = false;
    UINT m4xMSAAQuality = 0;

    GameTimer mGameTimer;

    Microsoft::WRL::ComPtr<IDXGIFactory4> mFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    
    UINT64 mCurrentFence = 0;
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    
    INT mCurrentBackBufferIndex = 0;
    static const INT SwapChainBufferSize = 2;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferSize];
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

    D3D12_VIEWPORT mScreenViewport;
    D3D12_RECT mScissorRect;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRTVHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDSVHeap;

    UINT mRTVDescriptorSize = 0;
    UINT mDSVDescriptorSize = 0;
    UINT mCBVSRVUAVDescriptorSize = 0;

    std::wstring mMainWindowTitle = L"Application Framework";
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    UINT mMainWindowWidth = 800;
    UINT mMainWindowHeight = 600;

    // camera object
};

#endif // APPLICATION_FRAMEWORK_H