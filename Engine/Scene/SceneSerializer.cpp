#include "pch.h"
#include "SceneSerializer.h"
#include "../Rendering/SkinnedMeshRenderer.h"
#include "../Graphics/MeshRenderer.h"
#include "../Animation/AnimatorComponent.h"
#include "../Audio/AudioSource.h"
#include "../Audio/AudioListener.h"
#include "../Core/CameraComponent.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace UnoEngine {

bool SceneSerializer::SaveScene(const std::vector<std::unique_ptr<GameObject>>& gameObjects, const std::string& filepath) {
    try {
        json sceneJson;
        sceneJson["scene_name"] = "Scene";
        sceneJson["version"] = "1.0";

        json objectsArray = json::array();
        for (const auto& obj : gameObjects) {
            if (obj) {
                objectsArray.push_back(SerializeGameObject(*obj));
            }
        }
        sceneJson["objects"] = objectsArray;

        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filepath << std::endl;
            return false;
        }

        file << sceneJson.dump(4); // Pretty print with 4-space indent
        file.close();

        std::cout << "Scene saved successfully: " << filepath << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving scene: " << e.what() << std::endl;
        return false;
    }
}

bool SceneSerializer::LoadScene(const std::string& filepath, std::vector<std::unique_ptr<GameObject>>& outGameObjects) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for reading: " << filepath << std::endl;
            return false;
        }

        json sceneJson;
        file >> sceneJson;
        file.close();

        // Clear existing objects
        outGameObjects.clear();

        // Load objects
        if (sceneJson.contains("objects")) {
            for (const auto& objJson : sceneJson["objects"]) {
                auto gameObject = DeserializeGameObject(objJson);
                if (gameObject) {
                    outGameObjects.push_back(std::move(gameObject));
                }
            }
        }

        std::cout << "Scene loaded successfully: " << filepath << std::endl;
        std::cout << "Loaded " << outGameObjects.size() << " objects" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading scene: " << e.what() << std::endl;
        return false;
    }
}

json SceneSerializer::SerializeGameObject(const GameObject& gameObject) {
    json obj;

    obj["name"] = gameObject.GetName();
    obj["active"] = gameObject.IsActive();
    obj["layer"] = gameObject.GetLayer();

    // Transform
    obj["transform"] = SerializeTransform(gameObject.GetTransform());

    // Components
    json componentsArray = json::array();
    for (const auto& component : gameObject.GetComponents()) {
        if (component) {
            json compJson = SerializeComponent(*component);
            if (!compJson.is_null()) {
                componentsArray.push_back(compJson);
            }
        }
    }
    obj["components"] = componentsArray;

    return obj;
}

std::unique_ptr<GameObject> SceneSerializer::DeserializeGameObject(const json& json) {
    auto gameObject = std::make_unique<GameObject>();

    if (json.contains("name")) {
        gameObject->SetName(json["name"].get<std::string>());
    }

    if (json.contains("active")) {
        gameObject->SetActive(json["active"].get<bool>());
    }

    if (json.contains("layer")) {
        gameObject->SetLayer(json["layer"].get<GameObject::Layer>());
    }

    // Transform
    if (json.contains("transform")) {
        DeserializeTransform(json["transform"], gameObject->GetTransform());
    }

    // Components
    if (json.contains("components")) {
        for (const auto& compJson : json["components"]) {
            DeserializeComponent(compJson, *gameObject);
        }
    }

    return gameObject;
}

json SceneSerializer::SerializeTransform(const Transform& transform) {
    json trans;

    auto pos = transform.GetLocalPosition();
    trans["position"] = {pos.GetX(), pos.GetY(), pos.GetZ()};

    auto rot = transform.GetLocalRotation();
    trans["rotation"] = {rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()};

    auto scale = transform.GetLocalScale();
    trans["scale"] = {scale.GetX(), scale.GetY(), scale.GetZ()};

    return trans;
}

void SceneSerializer::DeserializeTransform(const json& json, Transform& transform) {
    if (json.contains("position")) {
        auto pos = json["position"];
        transform.SetLocalPosition(Vector3(
            pos[0].get<float>(),
            pos[1].get<float>(),
            pos[2].get<float>()
        ));
    }

    if (json.contains("rotation")) {
        auto rot = json["rotation"];
        transform.SetLocalRotation(Quaternion(
            rot[0].get<float>(),
            rot[1].get<float>(),
            rot[2].get<float>(),
            rot[3].get<float>()
        ));
    }

    if (json.contains("scale")) {
        auto scale = json["scale"];
        transform.SetLocalScale(Vector3(
            scale[0].get<float>(),
            scale[1].get<float>(),
            scale[2].get<float>()
        ));
    }
}

json SceneSerializer::SerializeComponent(const Component& component) {
    json comp;

    // SkinnedMeshRenderer
    if (auto* renderer = dynamic_cast<const SkinnedMeshRenderer*>(&component)) {
        comp["type"] = "SkinnedMeshRenderer";
        comp["modelPath"] = renderer->GetModelPath();
        return comp;
    }

    // MeshRenderer (静的モデル用)
    if (auto* renderer = dynamic_cast<const MeshRenderer*>(&component)) {
        comp["type"] = "MeshRenderer";
        comp["modelPath"] = renderer->GetModelPath();
        return comp;
    }

    // AnimatorComponent
    if (auto* animator = dynamic_cast<const AnimatorComponent*>(&component)) {
        comp["type"] = "AnimatorComponent";
        return comp;
    }

    // AudioSource
    if (auto* audioSource = dynamic_cast<const AudioSource*>(&component)) {
        comp["type"] = "AudioSource";
        comp["clipPath"] = audioSource->GetClipPath();
        comp["volume"] = audioSource->GetVolume();
        comp["loop"] = audioSource->IsLooping();
        comp["playOnAwake"] = audioSource->GetPlayOnAwake();
        comp["is3D"] = audioSource->Is3D();
        comp["minDistance"] = audioSource->GetMinDistance();
        comp["maxDistance"] = audioSource->GetMaxDistance();
        return comp;
    }

    // AudioListener
    if (auto* audioListener = dynamic_cast<const AudioListener*>(&component)) {
        comp["type"] = "AudioListener";
        return comp;
    }

    // CameraComponent
    if (auto* camera = dynamic_cast<const CameraComponent*>(&component)) {
        comp["type"] = "CameraComponent";
        comp["fov"] = camera->GetFieldOfView();
        comp["aspect"] = camera->GetAspectRatio();
        comp["nearClip"] = camera->GetNearClip();
        comp["farClip"] = camera->GetFarClip();
        comp["isOrthographic"] = camera->IsOrthographic();
        comp["priority"] = camera->GetPriority();
        comp["isMain"] = camera->IsMain();
        return comp;
    }

    return json();
}

void SceneSerializer::DeserializeComponent(const json& json, GameObject& gameObject) {
    if (!json.contains("type")) {
        return;
    }

    std::string type = json["type"].get<std::string>();

    if (type == "SkinnedMeshRenderer") {
        auto* renderer = gameObject.AddComponent<SkinnedMeshRenderer>();
        if (json.contains("modelPath")) {
            std::string modelPath = json["modelPath"].get<std::string>();
            renderer->SetModel(modelPath);
        }
    }
    else if (type == "MeshRenderer") {
        auto* renderer = gameObject.AddComponent<MeshRenderer>();
        if (json.contains("modelPath")) {
            std::string modelPath = json["modelPath"].get<std::string>();
            renderer->SetModelPath(modelPath);
            // 注意: 実際のメッシュデータはScene::RestoreResources()で再ロードされる
        }
    }
    else if (type == "AnimatorComponent") {
        // AnimatorComponentはSkinnedMeshRendererによって自動的に追加・初期化される
    }
    else if (type == "AudioSource") {
        auto* audioSource = gameObject.AddComponent<AudioSource>();
        if (json.contains("clipPath")) {
            audioSource->SetClipPath(json["clipPath"].get<std::string>());
        }
        if (json.contains("volume")) {
            audioSource->SetVolume(json["volume"].get<float>());
        }
        if (json.contains("loop")) {
            audioSource->SetLoop(json["loop"].get<bool>());
        }
        if (json.contains("playOnAwake")) {
            audioSource->SetPlayOnAwake(json["playOnAwake"].get<bool>());
        }
        if (json.contains("is3D")) {
            audioSource->Set3D(json["is3D"].get<bool>());
        }
        if (json.contains("minDistance")) {
            audioSource->SetMinDistance(json["minDistance"].get<float>());
        }
        if (json.contains("maxDistance")) {
            audioSource->SetMaxDistance(json["maxDistance"].get<float>());
        }
    }
    else if (type == "AudioListener") {
        gameObject.AddComponent<AudioListener>();
    }
    else if (type == "CameraComponent") {
        auto* camera = gameObject.AddComponent<CameraComponent>();
        float fov = 60.0f * 0.0174533f;  // デフォルト値
        float aspect = 16.0f / 9.0f;
        float nearClip = 0.1f;
        float farClip = 1000.0f;

        if (json.contains("fov")) {
            fov = json["fov"].get<float>();
        }
        if (json.contains("aspect")) {
            aspect = json["aspect"].get<float>();
        }
        if (json.contains("nearClip")) {
            nearClip = json["nearClip"].get<float>();
        }
        if (json.contains("farClip")) {
            farClip = json["farClip"].get<float>();
        }

        bool isOrtho = false;
        if (json.contains("isOrthographic")) {
            isOrtho = json["isOrthographic"].get<bool>();
        }

        if (isOrtho) {
            camera->SetOrthographic(10.0f, 10.0f, nearClip, farClip);
        } else {
            camera->SetPerspective(fov, aspect, nearClip, farClip);
        }

        if (json.contains("priority")) {
            camera->SetPriority(json["priority"].get<int>());
        }
        if (json.contains("isMain")) {
            camera->SetMain(json["isMain"].get<bool>());
        }
    }
}

} // namespace UnoEngine
