#include "ApplicationFramework.h"

ApplicationFramework::ApplicationFramework()
{

}

ApplicationFramework::~ApplicationFramework()
{
    
}


bool ApplicationFramework::init()
{
    return true;
}

bool ApplicationFramework::run()
{
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