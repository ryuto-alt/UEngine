# GPU Mipmap Generation

## 1. Compute Shader (GenerateMips.hlsl)

```hlsl
SamplerState BilinearClamp : register(s0);
Texture2D<float4> SrcMip   : register(t0);
RWTexture2D<float4> OutMip : register(u0);

cbuffer MipConstants : register(b0)
{
    float2 InvOutTexelSize;
    uint SrcMipIndex;
    uint Padding;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float2 uv = (DTid.xy + 0.5) * InvOutTexelSize;
    OutMip[DTid.xy] = SrcMip.SampleLevel(BilinearClamp, uv, SrcMipIndex);
}
```

## 2. MipmapGenerator.h

```cpp
#pragma once
#include "D3D12Common.h"

class MipmapGenerator {
public:
    void Initialize(GraphicsDevice* graphics);
    void GenerateMips(ID3D12GraphicsCommandList* cmdList,
                      ID3D12Resource* texture,
                      D3D12_RESOURCE_STATES currentState);

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    uint32 m_descriptorSize = 0;
    uint32 m_currentTextureOffset = 0;  // 重要: テクスチャごとにユニークなオフセット

    static constexpr uint32 DESCRIPTORS_PER_MIP = 2;      // SRV + UAV
    static constexpr uint32 MAX_MIP_LEVELS = 16;
    static constexpr uint32 MAX_TEXTURES_PER_BATCH = 128; // 同時処理可能数
};
```

## 3. Root Signature 作成

```cpp
void MipmapGenerator::CreateRootSignature(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE1 srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_DESCRIPTOR_RANGE1 uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0,
                  D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParams[3];
    rootParams[0].InitAsConstants(4, 0);                    // cbuffer
    rootParams[1].InitAsDescriptorTable(1, &srvRange);      // SRV
    rootParams[2].InitAsDescriptorTable(1, &uavRange);      // UAV

    CD3DX12_STATIC_SAMPLER_DESC sampler(0,
        D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(3, rootParams, 1, &sampler);

    ComPtr<ID3DBlob> signature;
    D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, nullptr);
    device->CreateRootSignature(0, signature->GetBufferPointer(),
                                signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
}
```

## 4. Descriptor Heap 作成

```cpp
void MipmapGenerator::CreateDescriptorHeap(ID3D12Device* device) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    // 各テクスチャに専用領域を確保（衝突防止）
    desc.NumDescriptors = DESCRIPTORS_PER_MIP * MAX_MIP_LEVELS * MAX_TEXTURES_PER_BATCH;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeap));
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
```

## 5. Mipmap 生成（核心部分）

```cpp
void MipmapGenerator::GenerateMips(ID3D12GraphicsCommandList* cmdList,
                                   ID3D12Resource* texture,
                                   D3D12_RESOURCE_STATES currentState) {
    auto desc = texture->GetDesc();
    uint32 mipLevels = desc.MipLevels;
    if (mipLevels <= 1) return;

    // ★ポイント: テクスチャごとにユニークなオフセット
    uint32 baseOffset = m_currentTextureOffset * DESCRIPTORS_PER_MIP * MAX_MIP_LEVELS;
    m_currentTextureOffset = (m_currentTextureOffset + 1) % MAX_TEXTURES_PER_BATCH;

    cmdList->SetComputeRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->SetDescriptorHeaps(1, m_descriptorHeap.GetAddressOf());

    // Mip 0 を読み取り可能に
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture, currentState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0);
    cmdList->ResourceBarrier(1, &barrier);

    auto cpuHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto gpuHandle = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // 各ミップレベルを生成
    for (uint32 srcMip = 0; srcMip < mipLevels - 1; ++srcMip) {
        uint32 dstMip = srcMip + 1;
        uint32 dstW = std::max(1u, (uint32)desc.Width >> dstMip);
        uint32 dstH = std::max(1u, (uint32)desc.Height >> dstMip);

        // 出力先をUAV状態に
        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, dstMip);
        cmdList->ResourceBarrier(1, &barrier);

        // ディスクリプタ作成
        uint32 offset = baseOffset + srcMip * DESCRIPTORS_PER_MIP;

        D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = { cpuHandle.ptr + offset * m_descriptorSize };
        D3D12_CPU_DESCRIPTOR_HANDLE uavCpu = { cpuHandle.ptr + (offset + 1) * m_descriptorSize };
        D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = { gpuHandle.ptr + offset * m_descriptorSize };
        D3D12_GPU_DESCRIPTOR_HANDLE uavGpu = { gpuHandle.ptr + (offset + 1) * m_descriptorSize };

        // SRV: 全ミップアクセス可能
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = mipLevels;
        m_device->CreateShaderResourceView(texture, &srvDesc, srvCpu);

        // UAV: 出力先ミップのみ
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = desc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = dstMip;
        m_device->CreateUnorderedAccessView(texture, nullptr, &uavDesc, uavCpu);

        // 定数設定
        struct { float invSize[2]; uint32 srcMip; uint32 pad; } constants;
        constants.invSize[0] = 1.0f / dstW;
        constants.invSize[1] = 1.0f / dstH;
        constants.srcMip = srcMip;

        cmdList->SetComputeRoot32BitConstants(0, 4, &constants, 0);
        cmdList->SetComputeRootDescriptorTable(1, srvGpu);
        cmdList->SetComputeRootDescriptorTable(2, uavGpu);

        // 実行
        cmdList->Dispatch((dstW + 7) / 8, (dstH + 7) / 8, 1);

        // UAVバリア + 次のイテレーション用に状態遷移
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(texture));

        if (dstMip < mipLevels - 1) {
            barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, dstMip);
            cmdList->ResourceBarrier(1, &barrier);
        }
    }

    // 最終: 全ミップをPIXEL_SHADER_RESOURCEに
    // (省略: 各ミップの状態を追跡してバリア発行)
}
```

## 6. テクスチャロード時の呼び出し

```cpp
// Texture2D::LoadFromFile()
void Texture2D::LoadFromFile(GraphicsDevice* gfx, ID3D12GraphicsCommandList* cmdList,
                             const std::wstring& path, uint32 srvIndex) {
    // テクスチャ作成（UAVフラグ必須）
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;  // ★重要
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // sRGBではなくUNORM
    texDesc.MipLevels = CalculateMipLevels(width, height);
    // ... リソース作成 ...

    // ベースミップをアップロード
    UpdateSubresources(cmdList, resource, uploadBuffer, 0, 0, 1, &srcData);

    // GPU Mipmap生成
    gfx->GetMipmapGenerator()->GenerateMips(cmdList, resource, D3D12_RESOURCE_STATE_COPY_DEST);

    // SRV作成（sRGBとして解釈）
    gfx->CreateSRV(resource, srvIndex);
}
```

## 7. sRGB対応（SRV作成時）

```cpp
// GraphicsDevice::CreateSRV()
void GraphicsDevice::CreateSRV(ID3D12Resource* resource, uint32 index) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    DXGI_FORMAT format = resource->GetDesc().Format;

    // UNORMをsRGBとして解釈（ガンマ補正適用）
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }
    // ...
}
```

## 8. サンプラー設定

```cpp
// Pipeline.cpp
D3D12_STATIC_SAMPLER_DESC sampler = {};
sampler.Filter = D3D12_FILTER_ANISOTROPIC;
sampler.MaxAnisotropy = 16;
sampler.MipLODBias = -0.5f;  // ぼやけ防止
```

## ハマりポイント

| 問題 | 原因 | 解決 |
|-----|------|-----|
| 遠距離で真っ黒 | 複数テクスチャでディスクリプタ上書き | `m_currentTextureOffset`で分離 |
| UAV作成エラー | sRGBフォーマット非対応 | UNORM使用→SRVでsRGB解釈 |
| マテリアル薄い | sRGBガンマ補正なし | CreateSRVでsRGB変換 |
