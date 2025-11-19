#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "D3D12Common.h"
#include "Texture2D.h"
#include <string>
#include <memory>

namespace UnoEngine {

class GraphicsDevice;

struct MaterialData {
    std::string name;
    float ambient[3] = { 1.0f, 1.0f, 1.0f };
    float diffuse[3] = { 1.0f, 1.0f, 1.0f };
    float specular[3] = { 0.5f, 0.5f, 0.5f };
    float emissive[3] = { 0.0f, 0.0f, 0.0f };
    float shininess = 250.0f;
    float opacity = 1.0f;
    std::string diffuseTexturePath;

    // PBRパラメータ
    float metallic = 0.0f;   // 金属性 (0.0 = 非金属, 1.0 = 金属)
    float roughness = 0.5f;  // 粗さ (0.0 = 完全に滑らか, 1.0 = 完全に粗い)
    float albedo[3] = { 1.0f, 1.0f, 1.0f };  // アルベド色（基本色）
};

class Material : public NonCopyable {
public:
    Material() = default;
    ~Material() = default;
    Material(Material&&) = default;
    Material& operator=(Material&&) = default;

    void LoadFromData(const MaterialData& data, GraphicsDevice* graphics,
                     ID3D12GraphicsCommandList* commandList,
                     const std::string& baseDirectory, uint32 srvIndex);

    const MaterialData& GetData() const { return data_; }
    const Texture2D* GetDiffuseTexture() const { return diffuseTexture_.get(); }
    bool HasDiffuseTexture() const { return diffuseTexture_ != nullptr; }
    uint32 GetSRVIndex() const { return diffuseTexture_ ? diffuseTexture_->GetSRVIndex() : 0; }

private:
    MaterialData data_;
    std::unique_ptr<Texture2D> diffuseTexture_;
};

} // namespace UnoEngine
