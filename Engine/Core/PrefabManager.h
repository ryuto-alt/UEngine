#pragma once

#include "GameObject.h"
#include <string>
#include <memory>

namespace UnoEngine {

class Scene;

class PrefabManager {
public:
    /// Save a GameObject as a .prefab JSON file
    static bool SavePrefab(const GameObject* obj, const std::string& path);

    /// Instantiate a .prefab file into a scene (adds to scene's game objects)
    static std::unique_ptr<GameObject> LoadPrefab(const std::string& path);
};

} // namespace UnoEngine
