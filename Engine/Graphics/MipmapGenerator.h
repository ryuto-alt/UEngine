#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "D3D12Common.h"

namespace UnoEngine {

class GraphicsDevice;

class MipmapGenerator : public NonCopyable {
public:
    MipmapGenerator() = default;
    ~MipmapGenerator() = default;

    void Initialize(GraphicsDevice* graphics);
    
    void GenerateMips(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* texture,
        D3D12_RESOURCE_STATES currentState
    );

private:
    struct MipConstants {
        float invOutTexelSize[2];
        uint32 srcMipIndex;
        uint32 padding;
    };

    void CreateRootSignature(ID3D12Device* device);
    void CreatePipelineState(ID3D12Device* device);
    void CreateDescriptorHeap(ID3D12Device* device);

    GraphicsDevice* m_graphics = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    uint32 m_descriptorSize = 0;
    
    static constexpr uint32 DESCRIPTORS_PER_MIP = 2; // 1 SRV + 1 UAV
    static constexpr uint32 MAX_MIP_LEVELS = 16;
    static constexpr uint32 MAX_TEXTURES_PER_BATCH = 128;
    
    uint32 m_currentTextureOffset = 0; // Tracks offset for each texture in batch
};

} // namespace UnoEngine
