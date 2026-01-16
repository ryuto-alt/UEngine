#include "pch.h"
#include "Material.h"
#include "GraphicsDevice.h"
#include <filesystem>

namespace UnoEngine {

void Material::LoadFromData(const MaterialData& data, GraphicsDevice* graphics,
                           ID3D12GraphicsCommandList* commandList,
                           const std::string& baseDirectory, uint32 srvIndex) {
    data_ = data;
    device_ = graphics->GetDevice();

    if (!data_.diffuseTexturePath.empty()) {
        namespace fs = std::filesystem;

        fs::path texturePath(data_.diffuseTexturePath);

        if (!texturePath.is_absolute()) {
            texturePath = fs::path(baseDirectory) / texturePath;
        } else {
            fs::path filename = texturePath.filename();
            texturePath = fs::path(baseDirectory) / filename;
        }

        if (fs::exists(texturePath)) {
            diffuseTexture_ = std::make_unique<Texture2D>();
            diffuseTexture_->LoadFromFile(graphics, commandList, texturePath.wstring(), srvIndex);
            OutputDebugStringA(("[Material] Texture loaded: " + texturePath.string() + " SRV=" + std::to_string(srvIndex) + "\n").c_str());
        } else {
            OutputDebugStringA(("[Material] Texture NOT FOUND: " + texturePath.string() + "\n").c_str());
        }
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE Material::GetAlbedoSRV(ID3D12DescriptorHeap* heap) const {
    auto handle = heap->GetGPUDescriptorHandleForHeapStart();
    if (device_) {
        SIZE_T offset = 0;
        if (m_useDynamicTexture) {
            offset = m_dynamicSRVIndex;
        } else if (diffuseTexture_) {
            offset = diffuseTexture_->GetSRVIndex();
        }
        handle.ptr += offset * device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    return handle;
}

} // namespace UnoEngine
