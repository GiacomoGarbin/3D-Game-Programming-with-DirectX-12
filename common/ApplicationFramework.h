#ifndef APPLICATION_FRAMEWORK_H
#define APPLICATION_FRAMEWORK_H

// windows
#include <wrl.h>

// glfw
#include <glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw3native.h>

// directx
#include "d3dx12.h"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
using namespace DirectX;

// std library
#include <array>

// common
#include "utils.h"
#include "GameTimer.h"

class ApplicationFramework
{
public:
    ApplicationFramework();
    virtual ~ApplicationFramework();

    virtual bool init();
    bool run();

    //SetMSAA()
    //GetMSAA()

protected:
    virtual void CreateRTVAndDSVDescriptorHeaps();
    virtual void OnResize(GLFWwindow* window, int width, int height);
    virtual void update(GameTimer& timer) = 0;
    virtual void draw(GameTimer& timer) = 0;

    bool InitMainWindow();

    virtual void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
    virtual void OnMouseMove(GLFWwindow* window, double xpos, double ypos);
    virtual void OnKeyButton(GLFWwindow* window, int key, int scancode, int action, int mods);
    
    bool InitDirect3D();

    void CreateSwapChain();
    void CreateCommandObjects();
    void FlushCommandQueue();

    ID3D12Resource* GetCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

    //Microsoft::WRL::ComPtr<GLFWwindow> mMainWindow = nullptr;
    GLFWwindow* mMainWindow = nullptr;
    bool mApplicationPaused = false;

    bool m4xMSAAState = false;
    UINT m4xMSAAQuality = 0;

    GameTimer mGameTimer;

    Microsoft::WRL::ComPtr<IDXGIFactory> mFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
    
    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence = 0;

    static const INT SwapChainBufferSize = 2;
    INT mCurrentBackBufferIndex = 0;

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

    std::string mMainWindowTitle = "Application Framework";
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    UINT mMainWindowWidth = 800;
    UINT mMainWindowHeight = 600;

    std::array<bool, GLFW_KEY_LAST> mKeysState;
    XMFLOAT2 mLastMousePos;

    // camera object
};

#endif // APPLICATION_FRAMEWORK_H