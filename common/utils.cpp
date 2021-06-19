#include "utils.h"

UINT Utils::GetConstantBufferByteSize(UINT size)
{
    return (size + 255) & ~255;
}

Microsoft::WRL::ComPtr<ID3DBlob> Utils::CompileShader(const std::wstring& filename,
                                                      const D3D_SHADER_MACRO* defines,
                                                      const std::string& EntryPoint,
                                                      const std::string& target)
{
    UINT flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // DEBUG

    HRESULT hr = S_OK;

    Microsoft::WRL::ComPtr<ID3DBlob> ByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors = nullptr;

    hr = D3DCompileFromFile(filename.c_str(),
                            defines,
                            D3D_COMPILE_STANDARD_FILE_INCLUDE,
                            EntryPoint.c_str(),
                            target.c_str(),
                            flags,
                            0,
                            &ByteCode,
                            &errors);

    if (errors != nullptr)
    {
        OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    return ByteCode;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Utils::CreateDefaultBuffer(ID3D12Device* device,
                                                                  ID3D12GraphicsCommandList* CommandList,
                                                                  const void* data,
                                                                  UINT64 size,
                                                                  Microsoft::WRL::ComPtr<ID3D12Resource>& UploadBuffer)
{
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

    CD3DX12_HEAP_PROPERTIES properties;
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(device->CreateCommittedResource(&properties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &desc,
                                                  D3D12_RESOURCE_STATE_COMMON,
                                                  nullptr,
                                                  IID_PPV_ARGS(buffer.GetAddressOf())));

    properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    ThrowIfFailed(device->CreateCommittedResource(&properties,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &desc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ,
                                                  nullptr,
                                                  IID_PPV_ARGS(UploadBuffer.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA SubResourceData;
    SubResourceData.pData = data;
    SubResourceData.RowPitch = size;
    SubResourceData.SlicePitch = size;

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        CommandList->ResourceBarrier(1, &transition);
    }

    UpdateSubresources<1>(CommandList, buffer.Get(), UploadBuffer.Get(), 0, 0, 1, &SubResourceData);

    {
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
        CommandList->ResourceBarrier(1, &transition);
    }

    return buffer;
}

D3D12_VERTEX_BUFFER_VIEW MeshGeometry::GetVertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
    view.SizeInBytes = VertexBufferByteSize;
    view.StrideInBytes = VertexByteStride;

    return view;
}

D3D12_INDEX_BUFFER_VIEW MeshGeometry::GetIndexBufferView() const
{
    D3D12_INDEX_BUFFER_VIEW view;
    view.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
    view.SizeInBytes = IndexBufferByteSize;
    view.Format = IndexFormat;

    return view;
}

void MeshGeometry::DisposeUploaders()
{
    VertexBufferUploader = nullptr;
    IndexBufferUploader = nullptr;
}