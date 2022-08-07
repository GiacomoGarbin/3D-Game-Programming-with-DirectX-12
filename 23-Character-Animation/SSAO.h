#pragma once

#include "utils.h"
#include "FrameResource.h"

class SSAO
{
    ID3D12Device* mDevice;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;

    ID3D12PipelineState* mPSO = nullptr;
    ID3D12PipelineState* mBlurPSO = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMap;
    Microsoft::WRL::ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap;
    Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap0;
    Microsoft::WRL::ComPtr<ID3D12Resource> mAmbientMap1;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

    // need two for ping-ponging during blur
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

    UINT mRenderTargetWidth;
    UINT mRenderTargetHeight;

    DirectX::XMFLOAT4 mOffsets[14];

    D3D12_VIEWPORT mViewport;
    D3D12_RECT mScissorRect;

    void BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList,
                        FrameResource* pCurrentFrame,
                        const int count);
    void BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList,
                        const bool bIsHorizontalBlur);

    void BuildResources();
    void BuildRandomVectorTexture(ID3D12GraphicsCommandList* pCommandList);

    void BuildOffsetVectors();

public:
    SSAO(ID3D12Device* device,
         ID3D12GraphicsCommandList* pCommandList,
         const UINT width,
         const UINT height);

    static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;
    static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    static const int MaxBlurRadius = 5;

    UINT GetAmbientMapWidth() const;
    UINT GetAmbientMapHeight() const;

    void GetOffsetVectors(XMFLOAT4 offsets[14]);
    std::vector<float> CalcGaussWeights(float sigma);


    ID3D12Resource* GetNormalMap();
    ID3D12Resource* GetAmbientMap();

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetNormalMapRTV() const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetNormalMapSRV() const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetAmbientMapSRV() const;

    void BuildDescriptors(ID3D12Resource* pDepthStencilBuffer,
                          CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                          CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
                          CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
                          UINT cbvSrvUavDescriptorSize,
                          UINT rtvDescriptorSize);

    void RebuildDescriptors(ID3D12Resource* pDepthStencilBuffer);

    void SetPSOs(ID3D12PipelineState* pPSO, ID3D12PipelineState* pBlurPSO);

    void OnResize(UINT width, UINT height);

    ///<summary>
    /// Changes the render target to the Ambient render target and draws a fullscreen
    /// quad to kick off the pixel shader to compute the AmbientMap.  We still keep the
    /// main depth buffer binded to the pipeline, but depth buffer read/writes
    /// are disabled, as we do not need the depth buffer computing the Ambient map.
    ///</summary>
    void ComputeAmbientOcclusion(ID3D12GraphicsCommandList* pCommandList,
                                 FrameResource* pCurrentFrame,
                                 const int kBlurCount);

    void ClearAmbientMap(ID3D12GraphicsCommandList* pCommandList);
};