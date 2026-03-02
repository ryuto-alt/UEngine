#include "pch.h"
#include "MipmapGenerator.h"
#include "GraphicsDevice.h"
#include "d3dx12.h"
#include <d3dcompiler.h>

namespace UnoEngine {

void MipmapGenerator::Initialize(GraphicsDevice* graphics) {
    m_graphics = graphics;
    auto* device = graphics->GetDevice();

    CreateRootSignature(device);
    CreatePipelineState(device);
    CreateDescriptorHeap(device);
}

void MipmapGenerator::CreateRootSignature(ID3D12Device* device) {
    // Root parameter 0: Constants (invOutTexelSize, srcMipIndex)
    // Root parameter 1: SRV descriptor table
    // Root parameter 2: UAV descriptor table
    
    CD3DX12_DESCRIPTOR_RANGE1 srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    
    CD3DX12_DESCRIPTOR_RANGE1 uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParams[3];
    rootParams[0].InitAsConstants(4, 0); // 4 DWORDs: float2 + uint + uint
    rootParams[1].InitAsDescriptorTable(1, &srvRange);
    rootParams[2].InitAsDescriptorTable(1, &uavRange);

    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0, // register s0
        D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP
    );

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &sampler);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_1,
                                                        &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        ThrowIfFailed(hr, "Failed to serialize mipmap root signature");
    }

    ThrowIfFailed(
        device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                    IID_PPV_ARGS(&m_rootSignature)),
        "Failed to create mipmap root signature"
    );
}

void MipmapGenerator::CreatePipelineState(ID3D12Device* device) {
    ComPtr<ID3DBlob> computeShader;
    ComPtr<ID3DBlob> error;

    UINT compileFlags = 0;
#ifdef WITH_EDITOR
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        L"Shaders/GenerateMips.hlsl",
        nullptr, nullptr,
        "main", "cs_5_1",
        compileFlags, 0,
        &computeShader, &error
    );

    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        ThrowIfFailed(hr, "Failed to compile mipmap compute shader");
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };

    ThrowIfFailed(
        device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
        "Failed to create mipmap pipeline state"
    );
}

void MipmapGenerator::CreateDescriptorHeap(ID3D12Device* device) {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    // Each texture needs DESCRIPTORS_PER_MIP * MAX_MIP_LEVELS descriptors
    heapDesc.NumDescriptors = DESCRIPTORS_PER_MIP * MAX_MIP_LEVELS * MAX_TEXTURES_PER_BATCH;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ThrowIfFailed(
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)),
        "Failed to create mipmap descriptor heap"
    );

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void MipmapGenerator::GenerateMips(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* texture,
    D3D12_RESOURCE_STATES currentState
) {
    auto* device = m_graphics->GetDevice();
    auto resourceDesc = texture->GetDesc();
    uint32 mipLevels = static_cast<uint32>(resourceDesc.MipLevels);

    if (mipLevels <= 1) {
        if (currentState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                texture, currentState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &barrier);
        }
        return;
    }

    // Calculate base offset for this texture (each texture gets its own descriptor space)
    uint32 textureBaseOffset = m_currentTextureOffset * DESCRIPTORS_PER_MIP * MAX_MIP_LEVELS;
    m_currentTextureOffset = (m_currentTextureOffset + 1) % MAX_TEXTURES_PER_BATCH;

    commandList->SetComputeRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());

    ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    // Track state per mip level
    std::vector<D3D12_RESOURCE_STATES> mipStates(mipLevels, currentState);

    // Transition mip 0 to SRV state for reading
    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture, mipStates[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0
        );
        commandList->ResourceBarrier(1, &barrier);
        mipStates[0] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    auto cpuHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto gpuHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    for (uint32 srcMip = 0; srcMip < mipLevels - 1; ++srcMip) {
        uint32 dstMip = srcMip + 1;
        
        uint32 dstWidth = std::max(1u, static_cast<uint32>(resourceDesc.Width) >> dstMip);
        uint32 dstHeight = std::max(1u, static_cast<uint32>(resourceDesc.Height) >> dstMip);

        // Transition destination mip to UAV from its current state
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                texture, mipStates[dstMip], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, dstMip
            );
            commandList->ResourceBarrier(1, &barrier);
            mipStates[dstMip] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        // Use unique offset for this texture + mip level
        uint32 descOffset = textureBaseOffset + srcMip * DESCRIPTORS_PER_MIP;
        
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = cpuHandle;
        srvCpuHandle.ptr += descOffset * m_descriptorSize;
        
        D3D12_CPU_DESCRIPTOR_HANDLE uavCpuHandle = cpuHandle;
        uavCpuHandle.ptr += (descOffset + 1) * m_descriptorSize;

        D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = gpuHandle;
        srvGpuHandle.ptr += descOffset * m_descriptorSize;
        
        D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle = gpuHandle;
        uavGpuHandle.ptr += (descOffset + 1) * m_descriptorSize;

        // SRV for source mip
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = resourceDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = mipLevels;
        device->CreateShaderResourceView(texture, &srvDesc, srvCpuHandle);

        // UAV for destination mip
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = resourceDesc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = dstMip;
        device->CreateUnorderedAccessView(texture, nullptr, &uavDesc, uavCpuHandle);

        MipConstants constants = {};
        constants.invOutTexelSize[0] = 1.0f / static_cast<float>(dstWidth);
        constants.invOutTexelSize[1] = 1.0f / static_cast<float>(dstHeight);
        constants.srcMipIndex = srcMip;
        constants.padding = 0;

        commandList->SetComputeRoot32BitConstants(0, 4, &constants, 0);
        commandList->SetComputeRootDescriptorTable(1, srvGpuHandle);
        commandList->SetComputeRootDescriptorTable(2, uavGpuHandle);

        uint32 groupsX = std::max(1u, (dstWidth + 7) / 8);
        uint32 groupsY = std::max(1u, (dstHeight + 7) / 8);
        commandList->Dispatch(groupsX, groupsY, 1);

        // UAV barrier
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(texture);
            commandList->ResourceBarrier(1, &barrier);
        }

        // Transition destination mip to SRV for next iteration's read
        if (dstMip < mipLevels - 1) {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, dstMip
            );
            commandList->ResourceBarrier(1, &barrier);
            mipStates[dstMip] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        }
    }

    // Final transition: all mips to PIXEL_SHADER_RESOURCE
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(mipLevels);
    
    for (uint32 mip = 0; mip < mipLevels; ++mip) {
        if (mipStates[mip] != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                texture, mipStates[mip], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip
            ));
        }
    }
    
    if (!barriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }
}

} // namespace UnoEngine
