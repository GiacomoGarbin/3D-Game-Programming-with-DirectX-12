#ifndef APPLICATION_FRAMEWORK_H
#define APPLICATION_FRAMEWORK_H

#include <wrl.h>
#include <d3d12.h>
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

    bool m4xMSAAState = false;
    UINT m4xMSAAQuality = 0;

    GameTimer mGameTimer;

    Microsoft::WRL::ComPtr<ID3D12Device> mDevice;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandList> mCommandList;

};

#endif // APPLICATION_FRAMEWORK_H