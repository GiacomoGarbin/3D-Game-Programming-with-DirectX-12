#ifndef UTILS_H
#define UTILS_H

// windows
#include <wrl.h>
#include <comdef.h>

// imgui
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

// directx
#include "d3dx12.h"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>
#include <directxcollision.h>
using namespace DirectX;

// std library
#include <unordered_map>
#include <array>

// debug
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                                 \
{                                                                                        \
    HRESULT hr__ = (x);                                                                  \
    std::wstring wfn = AnsiToWString(__FILE__);                                          \
    if (FAILED(hr__)) { throw ApplicationFrameworkException(hr__, L#x, wfn, __LINE__); } \
}
#endif

struct ApplicationFrameworkException
{
    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring filename;
    int LineNumber = -1;

    ApplicationFrameworkException() = default;
    ApplicationFrameworkException(HRESULT hr, const std::wstring& funcname, const std::wstring& filename, int line) :
        ErrorCode(hr),
        FunctionName(funcname),
        filename(filename),
        LineNumber(line)
    {}

    std::wstring ToString() const
    {
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }
};

class Utils
{
public:
    static UINT GetConstantBufferByteSize(UINT size)
    {
        return (size * 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::wstring& filename,
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

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device,
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
};

struct SubMeshGeometry
{
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseIndexVertexLocation = 0;

    DirectX::BoundingBox BoundingBox;
};

struct MeshGeometry
{
    std::string name;

    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;

    std::unordered_map<std::string, SubMeshGeometry> DrawArgs;

    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW view;
        view.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        view.SizeInBytes = VertexBufferByteSize;
        view.StrideInBytes = VertexByteStride;

        return view;
    }

    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW view;
        view.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        view.SizeInBytes = IndexBufferByteSize;
        view.Format = IndexFormat;

        return view;
    }

    void DisposeUploaders()
    {
        VertexBufferUploader = nullptr;
        IndexBufferUploader = nullptr;
    }
};

template<typename T>
class UploadBuffer
{
    Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
    BYTE* mMappedData = nullptr;

    UINT mElementByteSize = 0;
    bool mIsConstantBuffer = false;

public:
    UploadBuffer(ID3D12Device* device, UINT count, bool IsConstantBuffer) :
        mIsConstantBuffer(IsConstantBuffer)
    {
        mElementByteSize = sizeof(T);

        if (IsConstantBuffer)
        {
            mElementByteSize = Utils::GetConstantBufferByteSize(sizeof(T));
        }

        CD3DX12_HEAP_PROPERTIES properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * count);

        ThrowIfFailed(device->CreateCommittedResource(&properties,
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &desc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr,
                                                      IID_PPV_ARGS(&mBuffer)));

        ThrowIfFailed(mBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
    }

    ~UploadBuffer()
    {
        if (mBuffer != nullptr)
        {
            mBuffer->Unmap(0, nullptr);
        }

        mMappedData = nullptr;
    }

    ID3D12Resource* GetResource() const
    {
        return mBuffer.Get();
    }

    void CopyData(int index, const T& data)
    {
        std::memcpy(&mMappedData[index * mElementByteSize], &data, sizeof(T));
    }
};

#endif // UTILS_H