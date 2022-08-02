#include "SSAO.h"
#include "directxpackedvector.h"

SSAO::SSAO(ID3D12Device* device,
     ID3D12GraphicsCommandList* pCommandList,
     const UINT width,
     const UINT height)
{
    mDevice = device;

    OnResize(width, height);

    BuildOffsetVectors();
    BuildRandomVectorTexture(pCommandList);
}

UINT SSAO::GetAmbientMapWidth() const
{
    return mRenderTargetWidth / 2;
}

UINT SSAO::GetAmbientMapHeight() const
{
    return mRenderTargetHeight / 2;
}

void SSAO::GetOffsetVectors(XMFLOAT4 offsets[14])
{
    std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);
}

std::vector<float> SSAO::CalcGaussWeights(float sigma)
{
    const float TwoSigmaSquare = 2.0f * sigma * sigma;

    const int radius = std::ceil(2.0f * sigma);

    assert(radius <= MaxBlurRadius);

    std::vector<float> weights;
    weights.resize(2 * radius + 1);

    float sum = 0.0f;

    for (int i = -radius; i <= radius; ++i)
    {
        float x = i;

        weights[i + radius] = expf(-x * x / TwoSigmaSquare);

        sum += weights[i + radius];
    }

    for (int i = 0; i < weights.size(); ++i)
    {
        weights[i] /= sum;
    }

    return weights;
}

ID3D12Resource* SSAO::GetNormalMap()
{
    return mNormalMap.Get();
}

ID3D12Resource* SSAO::GetAmbientMap()
{
    return mAmbientMap0.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAO::GetNormalMapRTV() const
{
    return mhNormalMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::GetNormalMapSRV() const
{
    return mhNormalMapGpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::GetAmbientMapSRV() const
{
    return mhAmbientMap0GpuSrv;
}

void SSAO::BuildDescriptors(ID3D12Resource* pDepthStencilBuffer,
                      CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                      CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
                      CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
                      UINT cbvSrvUavDescriptorSize,
                      UINT rtvDescriptorSize)
{
    // cache descriptors


    // The Ssao reserves heap space
    // for 5 contiguous Srvs.

    mhAmbientMap0CpuSrv = hCpuSrv;
    mhAmbientMap1CpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhNormalMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhDepthMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhRandomVectorMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mhAmbientMap0GpuSrv = hGpuSrv;
    mhAmbientMap1GpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhNormalMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhDepthMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhRandomVectorMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mhNormalMapCpuRtv = hCpuRtv;
    mhAmbientMap0CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);
    mhAmbientMap1CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);

    RebuildDescriptors(pDepthStencilBuffer);
}

void SSAO::RebuildDescriptors(ID3D12Resource* pDepthStencilBuffer)
{
    // SRV
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Format = NormalMapFormat;
        desc.Texture2D.MostDetailedMip = 0;
        desc.Texture2D.MipLevels = 1;
        mDevice->CreateShaderResourceView(mNormalMap.Get(), &desc, mhNormalMapCpuSrv);

        desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        mDevice->CreateShaderResourceView(pDepthStencilBuffer, &desc, mhDepthMapCpuSrv);

        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        mDevice->CreateShaderResourceView(mRandomVectorMap.Get(), &desc, mhRandomVectorMapCpuSrv);

        desc.Format = AmbientMapFormat;
        mDevice->CreateShaderResourceView(mAmbientMap0.Get(), &desc, mhAmbientMap0CpuSrv);
        mDevice->CreateShaderResourceView(mAmbientMap1.Get(), &desc, mhAmbientMap1CpuSrv);
    }

    // RTV
    {
        D3D12_RENDER_TARGET_VIEW_DESC desc = {};
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Format = NormalMapFormat;
        desc.Texture2D.MipSlice = 0;
        desc.Texture2D.PlaneSlice = 0;
        mDevice->CreateRenderTargetView(mNormalMap.Get(), &desc, mhNormalMapCpuRtv);

        desc.Format = AmbientMapFormat;
        mDevice->CreateRenderTargetView(mAmbientMap0.Get(), &desc, mhAmbientMap0CpuRtv);
        mDevice->CreateRenderTargetView(mAmbientMap1.Get(), &desc, mhAmbientMap1CpuRtv);
    }
}

void SSAO::SetPSOs(ID3D12PipelineState* pPSO, ID3D12PipelineState* pBlurPSO)
{
    mPSO = pPSO;
    mBlurPSO = pBlurPSO;
}

void SSAO::OnResize(UINT width, UINT height)
{
    if (mRenderTargetWidth != width || mRenderTargetHeight != height)
    {
        mRenderTargetWidth = width;
        mRenderTargetHeight = height;

        // we render to ambient map at half the resolution
        mViewport.TopLeftX = 0.0f;
        mViewport.TopLeftY = 0.0f;
        mViewport.Width = mRenderTargetWidth / 2.0f;
        mViewport.Height = mRenderTargetHeight / 2.0f;
        mViewport.MinDepth = 0.0f;
        mViewport.MaxDepth = 1.0f;

        mScissorRect = { 0, 0, static_cast<int>(mRenderTargetWidth) / 2, static_cast<int>(mRenderTargetHeight) / 2};

        BuildResources();
    }
}

void SSAO::ComputeAmbientOcclusion(ID3D12GraphicsCommandList* pCommandList,
                                   FrameResource* pCurrentFrame,
                                   const int kBlurCount)
{
    pCommandList->RSSetViewports(1, &mViewport);
    pCommandList->RSSetScissorRects(1, &mScissorRect);

    // we compute the initial SSAO to AmbientMap0

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(),
                                                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        pCommandList->ResourceBarrier(1, &transition);
    }

    float clear[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pCommandList->ClearRenderTargetView(mhAmbientMap0CpuRtv, clear, 0, nullptr);

    pCommandList->OMSetRenderTargets(1, &mhAmbientMap0CpuRtv, true, nullptr);

    // bind ambient occlusion constant buffer
    auto AmbientOcclusionCB = pCurrentFrame->AmbientOcclusionCB->GetResource();
    pCommandList->SetGraphicsRootConstantBufferView(0, AmbientOcclusionCB->GetGPUVirtualAddress());
    pCommandList->SetGraphicsRoot32BitConstant(1, 0, 0);

    // bind the normal and depth maps
    pCommandList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);
    // bind the random vector map
    pCommandList->SetGraphicsRootDescriptorTable(3, mhRandomVectorMapGpuSrv);

    pCommandList->SetPipelineState(mPSO);

    // draw fullscreen quad
    pCommandList->IASetVertexBuffers(0, 0, nullptr);
    pCommandList->IASetIndexBuffer(nullptr);
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->DrawInstanced(6, 1, 0, 0);

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(),
                                                                                   D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                                   D3D12_RESOURCE_STATE_GENERIC_READ);
        pCommandList->ResourceBarrier(1, &transition);
    }

    BlurAmbientMap(pCommandList, pCurrentFrame, kBlurCount);
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList,
                          FrameResource* pCurrentFrame,
                          const int count)
{
    pCommandList->SetPipelineState(mBlurPSO);

    auto ssaoCBAddress = pCurrentFrame->AmbientOcclusionCB->GetResource()->GetGPUVirtualAddress();
    pCommandList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);

    for (int i = 0; i < count; ++i)
    {
        BlurAmbientMap(pCommandList, true);
        BlurAmbientMap(pCommandList, false);
    }
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* pCommandList,
                          const bool bIsHorizontalBlur)
{
    ID3D12Resource* output = nullptr;
    CD3DX12_GPU_DESCRIPTOR_HANDLE srv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv;

    // ping-pong the two ambient map textures

    if (bIsHorizontalBlur == true)
    {
        output = mAmbientMap1.Get();
        srv = mhAmbientMap0GpuSrv;
        rtv = mhAmbientMap1CpuRtv;
        pCommandList->SetGraphicsRoot32BitConstant(1, 1, 0);
    }
    else
    {
        output = mAmbientMap0.Get();
        srv = mhAmbientMap1GpuSrv;
        rtv = mhAmbientMap0CpuRtv;
        pCommandList->SetGraphicsRoot32BitConstant(1, 0, 0);
    }

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(output,
                                                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                                   D3D12_RESOURCE_STATE_RENDER_TARGET);
        pCommandList->ResourceBarrier(1, &transition);
    }

    float clear[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pCommandList->ClearRenderTargetView(rtv, clear, 0, nullptr);

    pCommandList->OMSetRenderTargets(1, &rtv, true, nullptr);

    // bind the normal and depth maps
    pCommandList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);
    // bind the input ambient map
    pCommandList->SetGraphicsRootDescriptorTable(3, srv);

    // draw fullscreen quad
    pCommandList->IASetVertexBuffers(0, 0, nullptr);
    pCommandList->IASetIndexBuffer(nullptr);
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->DrawInstanced(6, 1, 0, 0);

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(output,
                                                                                   D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                                   D3D12_RESOURCE_STATE_GENERIC_READ);
        pCommandList->ResourceBarrier(1, &transition);
    }
}

void SSAO::BuildResources()
{
    // free the old resources if they exist
    mNormalMap = nullptr;
    mAmbientMap0 = nullptr;
    mAmbientMap1 = nullptr;

    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(D3D12_RESOURCE_DESC));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = mRenderTargetWidth;
    desc.Height = mRenderTargetHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = SSAO::NormalMapFormat;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);

    {
        float clear[] = { 0.0f, 0.0f, 1.0f, 0.0f };
        CD3DX12_CLEAR_VALUE ClearValue(NormalMapFormat, clear);

        ThrowIfFailed(mDevice->CreateCommittedResource(&properties,
                                                       D3D12_HEAP_FLAG_NONE,
                                                       &desc,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                                       &ClearValue,
                                                       IID_PPV_ARGS(&mNormalMap)));

        const std::string name = "SSAO_NormalMap";
        ThrowIfFailed(mNormalMap->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.data()));
    }

    // ambient occlusion maps are at half resolution
    desc.Width = mRenderTargetWidth / 2;
    desc.Height = mRenderTargetHeight / 2;
    desc.Format = SSAO::AmbientMapFormat;

    {
        float clear[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        CD3DX12_CLEAR_VALUE ClearValue(AmbientMapFormat, clear);

        {
            ThrowIfFailed(mDevice->CreateCommittedResource(&properties,
                                                           D3D12_HEAP_FLAG_NONE,
                                                           &desc,
                                                           D3D12_RESOURCE_STATE_GENERIC_READ,
                                                           &ClearValue,
                                                           IID_PPV_ARGS(&mAmbientMap0)));

            const std::string name = "SSAO_AmbientMap0";
            ThrowIfFailed(mAmbientMap0->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.data()));
        }

        {
            ThrowIfFailed(mDevice->CreateCommittedResource(&properties,
                                                           D3D12_HEAP_FLAG_NONE,
                                                           &desc,
                                                           D3D12_RESOURCE_STATE_GENERIC_READ,
                                                           &ClearValue,
                                                           IID_PPV_ARGS(&mAmbientMap1)));

            const std::string name = "SSAO_AmbientMap1";
            ThrowIfFailed(mAmbientMap1->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.data()));
        }
    }
}

void SSAO::BuildRandomVectorTexture(ID3D12GraphicsCommandList* pCommandList)
{
    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(D3D12_RESOURCE_DESC));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = 256;
    desc.Height = 256;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES HeapTypeDefault(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES HeapTypeUpload(D3D12_HEAP_TYPE_UPLOAD);

    ThrowIfFailed(mDevice->CreateCommittedResource(&HeapTypeDefault,
                                                   D3D12_HEAP_FLAG_NONE,
                                                   &desc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr,
                                                   IID_PPV_ARGS(&mRandomVectorMap)));

    const std::string name = "SSAO_RandomVectorMap";
    ThrowIfFailed(mRandomVectorMap->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.data()));

    const UINT subresources = desc.DepthOrArraySize * desc.MipLevels;
    const UINT64 UploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap.Get(), 0, subresources);

    auto buffer = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(&HeapTypeUpload,
                                                   D3D12_HEAP_FLAG_NONE,
                                                   &buffer,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr,
                                                   IID_PPV_ARGS(mRandomVectorMapUploadBuffer.GetAddressOf())));

    //const std::string name = "SSAO_RandomVectorMapUploadBuffer";
    //ThrowIfFailed(mRandomVectorMapUploadBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.data()));

    DirectX::PackedVector::XMCOLOR vectors[256 * 256];

    for (int i = 0; i < 256; ++i)
    {
        for (int j = 0; j < 256; ++j)
        {
            // random vector in [0,1]
            XMFLOAT3 v(MathHelper::RandFloat(), MathHelper::RandFloat(), MathHelper::RandFloat());

            vectors[i * 256 + j] = DirectX::PackedVector::XMCOLOR(v.x, v.y, v.z, 0.0f);
        }
    }

    D3D12_SUBRESOURCE_DATA data = {};
    data.pData = vectors;
    data.RowPitch = 256 * sizeof(DirectX::PackedVector::XMCOLOR);
    data.SlicePitch = data.RowPitch * 256;

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(),
                                                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                                   D3D12_RESOURCE_STATE_COPY_DEST);
        pCommandList->ResourceBarrier(1, &transition);
    }
    
    UpdateSubresources(pCommandList,
                       mRandomVectorMap.Get(),
                       mRandomVectorMapUploadBuffer.Get(),
                       0,
                       0,
                       subresources,
                       &data);

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(),
                                                                                   D3D12_RESOURCE_STATE_COPY_DEST,
                                                                                   D3D12_RESOURCE_STATE_GENERIC_READ);
        pCommandList->ResourceBarrier(1, &transition);
    }

}

void SSAO::BuildOffsetVectors()
{
    // 8 cube corners
    mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

    mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

    // 6 centers of cube faces
    mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
    mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

    mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
    mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

    mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
    mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

    for (int i = 0; i < 14; ++i)
    {
        // random lengths in [0.25, 1.0]
        float s = MathHelper::RandFloat(0.25f, 1.0f);

        XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

        XMStoreFloat4(&mOffsets[i], v);
    }
}
