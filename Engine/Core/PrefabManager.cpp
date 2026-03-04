#include "pch.h"
#include "PrefabManager.h"
#include "../Scene/SceneSerializer.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>

namespace UnoEngine {

bool PrefabManager::SavePrefab(const GameObject* obj, const std::string& path) {
    if (!obj) return false;

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream file(path);
    if (!file.is_open()) {
        Logger::Error("[PrefabManager] Cannot open for write: {}", path);
        return false;
    }

    file << SceneSerializer::SerializeSingleObject(*obj);
    Logger::Info("[PrefabManager] Saved prefab: {}", path);
    return true;
}

std::unique_ptr<GameObject> PrefabManager::LoadPrefab(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::Error("[PrefabManager] Cannot open prefab: {}", path);
        return nullptr;
    }

    std::string jsonStr((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    auto obj = SceneSerializer::DeserializeSingleObject(jsonStr);
    if (obj) {
        Logger::Info("[PrefabManager] Loaded prefab: {}", path);
    }
    return obj;
}

} // namespace UnoEngine
