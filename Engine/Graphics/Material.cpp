#include "Material.h"
#include "GraphicsDevice.h"
#include <filesystem>

namespace UnoEngine {

void Material::LoadFromData(const MaterialData& data, GraphicsDevice* graphics,
                           ID3D12GraphicsCommandList* commandList,
                           const std::string& baseDirectory, uint32 srvIndex) {
    data_ = data;

    char debugMsg[512];
    sprintf_s(debugMsg, "Material::LoadFromData - TexturePath='%s', BaseDir='%s', SRVIndex=%u\n", data_.diffuseTexturePath.c_str(), baseDirectory.c_str(), srvIndex);
    OutputDebugStringA(debugMsg);

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
            
            char successMsg[512];
            sprintf_s(successMsg, "Material: Texture loaded successfully from '%s'\n", texturePath.string().c_str());
            OutputDebugStringA(successMsg);
        } else {
            char errorMsg[512];
            sprintf_s(errorMsg, "Material: Texture file NOT FOUND: '%s'\n", texturePath.string().c_str());
            OutputDebugStringA(errorMsg);
        }
    }
}

} // namespace UnoEngine
