#ifndef APPLICATION_FRAMEWORK_H
#define APPLICATION_FRAMEWORK_H

#include <wrl.h>
#include <d3d12.h>
#include <dxgi.h>
#include <DirectXColors.h>
#include "d3dx12.h"
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
    virtual void OnResize();
    virtual void update(GameTimer& timer) = 0;
    virtual void draw(GameTimer& timer) = 0;

    // mouse events handler
    
    bool InitMainWindow();
    bool InitDirect3D();

    void CreateCommandObjects();
    void CreateSwapChain();

    void FlushCommandQueue();

    ID3D12Resource* GetCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;

    bool m4xMSAAState = false;
    UINT m4xMSAAQuality = 0;

    GameTimer mGameTimer;

    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;

    static const INT SwapChainBufferSize = 2;
    INT mCurrentBackBufferIndex = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferSize];

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

    D3D12_VIEWPORT mScreenViewport;
    D3D12_RECT mScissorRect;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRTVHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDSVHeap;

    UINT mRTVDescriptorSize = 0;
    UINT mDSVDescriptorSize = 0;

};

#endif // APPLICATION_FRAMEWORK_H