#include "GameApplication.h"
#include "../Engine/Graphics/ResourceLoader.h"

namespace UnoEngine {

void GameApplication::OnInit() {
    // Game-specific initialization
}

Mesh* GameApplication::LoadMesh(const std::string& path) {
    return ResourceLoader::LoadMesh(path);
}

Material* GameApplication::LoadMaterial(const std::string& name) {
    return ResourceLoader::LoadMaterial(name);
}

} // namespace UnoEngine
