#ifndef APPLICATION_FRAMEWORK_H
#define APPLICATION_FRAMEWORK_H

#include "GameTimer.h"

class ApplicationFramework
{
public:
    ApplicationFramework();
    virtual ~ApplicationFramework();

    bool init();
    bool run();

    //SetMSAA()
    //GetMSAA()

protected:
    virtual void CreateRTVAndDSTDescriptorHeaps();
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
};

#endif // APPLICATION_FRAMEWORK_H