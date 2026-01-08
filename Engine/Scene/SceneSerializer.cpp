#include "pch.h"
#include "SceneSerializer.h"
#include "../Rendering/SkinnedMeshRenderer.h"
#include "../Graphics/MeshRenderer.h"
#include "../Animation/AnimatorComponent.h"
#include "../Audio/AudioSource.h"
#include "../Audio/AudioListener.h"
#include "../Core/CameraComponent.h"
#include "../Core/CollisionComponent.h"
#include "../Scripting/LuaScriptComponent.h"
#include "../Navigation/NavAgentComponent.h"
#include "../Navigation/NavMeshManager.h"
#include "../PostProcess/PostProcessType.h"
#include <fstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;

namespace UnoEngine {

bool SceneSerializer::SaveScene(const std::vector<std::unique_ptr<GameObject>>& gameObjects, const std::string& filepath) {
    try {
        json sceneJson;
        sceneJson["scene_name"] = "Scene";
        sceneJson["version"] = "1.1";

        json objectsArray = json::array();
        for (const auto& obj : gameObjects) {
            if (obj) {
                objectsArray.push_back(SerializeGameObject(*obj));
            }
        }
        sceneJson["objects"] = objectsArray;

        // NavMesh設定を保存
        auto& navMesh = Navigation::NavMeshManager::Get();
        const auto& settings = navMesh.GetSettings();
        
        json navMeshJson;
        navMeshJson["cellSize"] = settings.cellSize;
        navMeshJson["cellHeight"] = settings.cellHeight;
        navMeshJson["agentRadius"] = settings.agentRadius;
        navMeshJson["agentHeight"] = settings.agentHeight;
        navMeshJson["agentMaxClimb"] = settings.agentMaxClimb;
        navMeshJson["agentMaxSlope"] = settings.agentMaxSlope;
        navMeshJson["maxSimplificationError"] = settings.maxSimplificationError;
        navMeshJson["detailSampleDist"] = settings.detailSampleDist;
        navMeshJson["detailSampleMaxError"] = settings.detailSampleMaxError;
        navMeshJson["minRegionArea"] = settings.minRegionArea;
        navMeshJson["mergeRegionArea"] = settings.mergeRegionArea;
        navMeshJson["maxEdgeLength"] = settings.maxEdgeLength;
        navMeshJson["maxVertsPerPoly"] = settings.maxVertsPerPoly;
        navMeshJson["maxTiles"] = settings.maxTiles;
        navMeshJson["tileSize"] = settings.tileSize;
        navMeshJson["useMonotone"] = settings.useMonotone;
        navMeshJson["filterLowHangingObstacles"] = settings.filterLowHangingObstacles;
        navMeshJson["filterLedgeSpans"] = settings.filterLedgeSpans;
        navMeshJson["filterWalkableLowHeightSpans"] = settings.filterWalkableLowHeightSpans;
        
        // NavMeshがビルド済みの場合、バイナリファイルパスを設定
        if (navMesh.IsBuilt()) {
            // シーンファイルと同じディレクトリに.navmeshファイルを保存
            std::filesystem::path scenePath(filepath);
            std::string navMeshPath = scenePath.parent_path().string() + "/" + 
                                      scenePath.stem().string() + ".navmesh";
            navMeshJson["dataPath"] = navMeshPath;
            
            // NavMeshバイナリを保存
            if (navMesh.SaveNavMesh(navMeshPath)) {
                std::cout << "NavMesh saved: " << navMeshPath << std::endl;
            }
        }
        
        sceneJson["navmesh"] = navMeshJson;

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

        // NavMesh設定を読み込み
        if (sceneJson.contains("navmesh")) {
            auto& navMesh = Navigation::NavMeshManager::Get();
            const auto& navMeshJson = sceneJson["navmesh"];
            
            Navigation::NavMeshBuildSettings settings;
            if (navMeshJson.contains("cellSize")) settings.cellSize = navMeshJson["cellSize"].get<float>();
            if (navMeshJson.contains("cellHeight")) settings.cellHeight = navMeshJson["cellHeight"].get<float>();
            if (navMeshJson.contains("agentRadius")) settings.agentRadius = navMeshJson["agentRadius"].get<float>();
            if (navMeshJson.contains("agentHeight")) settings.agentHeight = navMeshJson["agentHeight"].get<float>();
            if (navMeshJson.contains("agentMaxClimb")) settings.agentMaxClimb = navMeshJson["agentMaxClimb"].get<float>();
            if (navMeshJson.contains("agentMaxSlope")) settings.agentMaxSlope = navMeshJson["agentMaxSlope"].get<float>();
            if (navMeshJson.contains("maxSimplificationError")) settings.maxSimplificationError = navMeshJson["maxSimplificationError"].get<float>();
            if (navMeshJson.contains("detailSampleDist")) settings.detailSampleDist = navMeshJson["detailSampleDist"].get<float>();
            if (navMeshJson.contains("detailSampleMaxError")) settings.detailSampleMaxError = navMeshJson["detailSampleMaxError"].get<float>();
            if (navMeshJson.contains("minRegionArea")) settings.minRegionArea = navMeshJson["minRegionArea"].get<int>();
            if (navMeshJson.contains("mergeRegionArea")) settings.mergeRegionArea = navMeshJson["mergeRegionArea"].get<int>();
            if (navMeshJson.contains("maxEdgeLength")) settings.maxEdgeLength = navMeshJson["maxEdgeLength"].get<int>();
            if (navMeshJson.contains("maxVertsPerPoly")) settings.maxVertsPerPoly = navMeshJson["maxVertsPerPoly"].get<int>();
            if (navMeshJson.contains("maxTiles")) settings.maxTiles = navMeshJson["maxTiles"].get<int>();
            if (navMeshJson.contains("tileSize")) settings.tileSize = navMeshJson["tileSize"].get<int>();
            if (navMeshJson.contains("useMonotone")) settings.useMonotone = navMeshJson["useMonotone"].get<bool>();
            if (navMeshJson.contains("filterLowHangingObstacles")) settings.filterLowHangingObstacles = navMeshJson["filterLowHangingObstacles"].get<bool>();
            if (navMeshJson.contains("filterLedgeSpans")) settings.filterLedgeSpans = navMeshJson["filterLedgeSpans"].get<bool>();
            if (navMeshJson.contains("filterWalkableLowHeightSpans")) settings.filterWalkableLowHeightSpans = navMeshJson["filterWalkableLowHeightSpans"].get<bool>();
            
            navMesh.SetSettings(settings);
            
            // NavMeshバイナリを読み込み
            if (navMeshJson.contains("dataPath")) {
                std::string navMeshPath = navMeshJson["dataPath"].get<std::string>();
                if (navMesh.LoadNavMesh(navMeshPath)) {
                    std::cout << "NavMesh loaded: " << navMeshPath << std::endl;
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
        comp["postProcessEnabled"] = camera->IsPostProcessEnabled();
        comp["postProcessIntensity"] = camera->GetPostProcessIntensity();

        // 複数エフェクトチェーンを保存
        json effectsArray = json::array();
        for (auto effect : camera->GetPostProcessEffects()) {
            effectsArray.push_back(static_cast<int>(effect));
        }
        comp["postProcessEffects"] = effectsArray;

        // Vignette params
        const auto& vignetteParams = camera->GetVignetteParams();
        comp["vignetteRadius"] = vignetteParams.radius;
        comp["vignetteSoftness"] = vignetteParams.softness;
        comp["vignetteIntensity"] = vignetteParams.intensity;

        // Fisheye params
        const auto& fisheyeParams = camera->GetFisheyeParams();
        comp["fisheyeStrength"] = fisheyeParams.strength;
        comp["fisheyeZoom"] = fisheyeParams.zoom;

        // Grayscale params
        const auto& grayscaleParams = camera->GetGrayscaleParams();
        comp["grayscaleIntensity"] = grayscaleParams.intensity;

        // PS1 params
        const auto& ps1Params = camera->GetPS1Params();
        comp["ps1ColorDepth"] = ps1Params.colorDepth;
        comp["ps1ResolutionScale"] = ps1Params.resolutionScale;
        comp["ps1DitherEnabled"] = ps1Params.ditherEnabled;
        comp["ps1DitherStrength"] = ps1Params.ditherStrength;

        // ChromaticAberration params
        const auto& caParams = camera->GetChromaticAberrationParams();
        comp["caIntensity"] = caParams.intensity;
        comp["caRedOffset"] = caParams.redOffset;
        comp["caGreenOffset"] = caParams.greenOffset;
        comp["caBlueOffset"] = caParams.blueOffset;

        // カメラ追従設定
        comp["viewMode"] = static_cast<int>(camera->GetViewMode());
        comp["followTargetName"] = camera->GetFollowTargetName();
        comp["followDistance"] = camera->GetFollowDistance();
        comp["followHeight"] = camera->GetFollowHeight();
        comp["followPitch"] = camera->GetFollowPitch();
        auto fpOffset = camera->GetFirstPersonOffset();
        comp["firstPersonOffset"] = { fpOffset.GetX(), fpOffset.GetY(), fpOffset.GetZ() };
        comp["followSmoothness"] = camera->GetFollowSmoothness();
        comp["mouseSensitivity"] = camera->GetMouseSensitivity();
        comp["firstPersonMoveSpeed"] = camera->GetFirstPersonMoveSpeed();
        comp["hideTargetInFirstPerson"] = camera->GetHideTargetInFirstPerson();

        return comp;
    }

    // CollisionComponent
    if (auto* collision = dynamic_cast<const CollisionComponent*>(&component)) {
        comp["type"] = "CollisionComponent";
        comp["enabled"] = collision->IsEnabled();
        comp["isTrigger"] = collision->IsTrigger();
        comp["isStatic"] = collision->IsStatic();
        comp["navMeshArea"] = static_cast<int>(collision->GetNavMeshArea());
        comp["autoSize"] = collision->IsAutoSized();
        comp["collisionLayer"] = collision->GetCollisionLayer();
        comp["collisionMask"] = collision->GetCollisionMask();

        const auto& aabb = collision->GetLocalAABB();
        comp["localAABBMin"] = { aabb.min.GetX(), aabb.min.GetY(), aabb.min.GetZ() };
        comp["localAABBMax"] = { aabb.max.GetX(), aabb.max.GetY(), aabb.max.GetZ() };

        // Save multiple AABBs if present
        const auto& localAABBs = collision->GetLocalAABBs();
        if (!localAABBs.empty()) {
            json aabbsArray = json::array();
            for (const auto& box : localAABBs) {
                json boxJson;
                boxJson["min"] = { box.min.GetX(), box.min.GetY(), box.min.GetZ() };
                boxJson["max"] = { box.max.GetX(), box.max.GetY(), box.max.GetZ() };
                aabbsArray.push_back(boxJson);
            }
            comp["localAABBs"] = aabbsArray;
        }

        return comp;
    }

    // LuaScriptComponent
    if (auto* luaScript = dynamic_cast<const LuaScriptComponent*>(&component)) {
        comp["type"] = "LuaScriptComponent";
        comp["scriptPath"] = luaScript->GetScriptPath();
        
        // プロパティを保存
        auto properties = luaScript->GetProperties();
        if (!properties.empty()) {
            json propsJson = json::array();
            for (const auto& prop : properties) {
                json propJson;
                propJson["name"] = prop.name;
                std::visit([&propJson](auto&& val) {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, bool>) {
                        propJson["type"] = "bool";
                        propJson["value"] = val;
                    } else if constexpr (std::is_same_v<T, int32>) {
                        propJson["type"] = "int";
                        propJson["value"] = val;
                    } else if constexpr (std::is_same_v<T, float>) {
                        propJson["type"] = "float";
                        propJson["value"] = val;
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        propJson["type"] = "string";
                        propJson["value"] = val;
                    }
                }, prop.value);
                propsJson.push_back(propJson);
            }
            comp["properties"] = propsJson;
        }
        return comp;
    }

    // NavAgentComponent
    if (auto* navAgent = dynamic_cast<const NavAgentComponent*>(&component)) {
        comp["type"] = "NavAgentComponent";
        comp["speed"] = navAgent->GetSpeed();
        comp["angularSpeed"] = navAgent->GetAngularSpeed();
        comp["acceleration"] = navAgent->GetAcceleration();
        comp["stoppingDistance"] = navAgent->GetStoppingDistance();
        comp["agentRadius"] = navAgent->GetAgentRadius();
        comp["agentHeight"] = navAgent->GetAgentHeight();
        comp["useCrowd"] = navAgent->IsUsingCrowd();
        comp["waitTime"] = navAgent->GetWaitTime();
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
        if (json.contains("postProcessEnabled")) {
            camera->SetPostProcessEnabled(json["postProcessEnabled"].get<bool>());
        }
        // 複数エフェクトチェーンを読み込み
        if (json.contains("postProcessEffects") && json["postProcessEffects"].is_array()) {
            std::vector<PostProcessType> effects;
            for (const auto& e : json["postProcessEffects"]) {
                effects.push_back(static_cast<PostProcessType>(e.get<int>()));
            }
            camera->SetPostProcessEffects(effects);
        }
        // 旧フォーマット互換（単一エフェクト）
        else if (json.contains("postProcessEffect")) {
            camera->SetPostProcessEffect(static_cast<PostProcessType>(json["postProcessEffect"].get<int>()));
        }
        if (json.contains("postProcessIntensity")) {
            camera->SetPostProcessIntensity(json["postProcessIntensity"].get<float>());
        }

        // Vignette params
        VignetteParams vignetteParams;
        if (json.contains("vignetteRadius")) {
            vignetteParams.radius = json["vignetteRadius"].get<float>();
        }
        if (json.contains("vignetteSoftness")) {
            vignetteParams.softness = json["vignetteSoftness"].get<float>();
        }
        if (json.contains("vignetteIntensity")) {
            vignetteParams.intensity = json["vignetteIntensity"].get<float>();
        }
        camera->SetVignetteParams(vignetteParams);

        // Fisheye params
        FisheyeParams fisheyeParams;
        if (json.contains("fisheyeStrength")) {
            fisheyeParams.strength = json["fisheyeStrength"].get<float>();
        }
        if (json.contains("fisheyeZoom")) {
            fisheyeParams.zoom = json["fisheyeZoom"].get<float>();
        }
        camera->SetFisheyeParams(fisheyeParams);

        // Grayscale params
        GrayscaleParams grayscaleParams;
        if (json.contains("grayscaleIntensity")) {
            grayscaleParams.intensity = json["grayscaleIntensity"].get<float>();
        }
        camera->SetGrayscaleParams(grayscaleParams);

        // PS1 params
        PS1Params ps1Params;
        if (json.contains("ps1ColorDepth")) {
            ps1Params.colorDepth = json["ps1ColorDepth"].get<std::uint32_t>();
        }
        if (json.contains("ps1ResolutionScale")) {
            ps1Params.resolutionScale = json["ps1ResolutionScale"].get<float>();
        }
        if (json.contains("ps1DitherEnabled")) {
            ps1Params.ditherEnabled = json["ps1DitherEnabled"].get<bool>();
        }
        if (json.contains("ps1DitherStrength")) {
            ps1Params.ditherStrength = json["ps1DitherStrength"].get<float>();
        }
        camera->SetPS1Params(ps1Params);

        // ChromaticAberration params
        ChromaticAberrationParams caParams;
        if (json.contains("caIntensity")) {
            caParams.intensity = json["caIntensity"].get<float>();
        }
        if (json.contains("caRedOffset")) {
            caParams.redOffset = json["caRedOffset"].get<float>();
        }
        if (json.contains("caGreenOffset")) {
            caParams.greenOffset = json["caGreenOffset"].get<float>();
        }
        if (json.contains("caBlueOffset")) {
            caParams.blueOffset = json["caBlueOffset"].get<float>();
        }
        camera->SetChromaticAberrationParams(caParams);

        // カメラ追従設定
        if (json.contains("viewMode")) {
            camera->SetViewMode(static_cast<CameraViewMode>(json["viewMode"].get<int>()));
        }
        if (json.contains("followTargetName")) {
            camera->SetFollowTargetName(json["followTargetName"].get<std::string>());
        }
        if (json.contains("followDistance")) {
            camera->SetFollowDistance(json["followDistance"].get<float>());
        }
        if (json.contains("followHeight")) {
            camera->SetFollowHeight(json["followHeight"].get<float>());
        }
        if (json.contains("followPitch")) {
            camera->SetFollowPitch(json["followPitch"].get<float>());
        }
        if (json.contains("firstPersonOffset") && json["firstPersonOffset"].is_array()) {
            auto offset = json["firstPersonOffset"];
            camera->SetFirstPersonOffset(Vector3(
                offset[0].get<float>(),
                offset[1].get<float>(),
                offset[2].get<float>()
            ));
        }
        if (json.contains("followSmoothness")) {
            camera->SetFollowSmoothness(json["followSmoothness"].get<float>());
        }
        if (json.contains("mouseSensitivity")) {
            camera->SetMouseSensitivity(json["mouseSensitivity"].get<float>());
        }
        if (json.contains("firstPersonMoveSpeed")) {
            camera->SetFirstPersonMoveSpeed(json["firstPersonMoveSpeed"].get<float>());
        }
        if (json.contains("hideTargetInFirstPerson")) {
            camera->SetHideTargetInFirstPerson(json["hideTargetInFirstPerson"].get<bool>());
        }
    }
    else if (type == "CollisionComponent") {
        auto* collision = gameObject.AddComponent<CollisionComponent>();

        if (json.contains("enabled")) {
            collision->SetEnabled(json["enabled"].get<bool>());
        }
        if (json.contains("isTrigger")) {
            collision->SetTrigger(json["isTrigger"].get<bool>());
        }
        if (json.contains("isStatic")) {
            collision->SetStatic(json["isStatic"].get<bool>());
        }
        if (json.contains("navMeshArea")) {
            collision->SetNavMeshArea(static_cast<NavMeshAreaType>(json["navMeshArea"].get<int>()));
        }
        if (json.contains("autoSize")) {
            collision->SetAutoSize(json["autoSize"].get<bool>());
        }
        if (json.contains("collisionLayer")) {
            collision->SetCollisionLayer(json["collisionLayer"].get<uint32_t>());
        }
        if (json.contains("collisionMask")) {
            collision->SetCollisionMask(json["collisionMask"].get<uint32_t>());
        }
        if (json.contains("localAABBMin") && json.contains("localAABBMax")) {
            auto minArr = json["localAABBMin"];
            auto maxArr = json["localAABBMax"];
            if (minArr.is_array() && maxArr.is_array() && minArr.size() >= 3 && maxArr.size() >= 3) {
                Vector3 min(minArr[0].get<float>(), minArr[1].get<float>(), minArr[2].get<float>());
                Vector3 max(maxArr[0].get<float>(), maxArr[1].get<float>(), maxArr[2].get<float>());
                collision->SetLocalAABB(min, max);
            }
        }
        // Load multiple AABBs if present
        if (json.contains("localAABBs") && json["localAABBs"].is_array()) {
            std::vector<AABB> aabbs;
            for (const auto& boxJson : json["localAABBs"]) {
                if (boxJson.contains("min") && boxJson.contains("max")) {
                    auto minArr = boxJson["min"];
                    auto maxArr = boxJson["max"];
                    if (minArr.is_array() && maxArr.is_array() && minArr.size() >= 3 && maxArr.size() >= 3) {
                        AABB box;
                        box.min = Vector3(minArr[0].get<float>(), minArr[1].get<float>(), minArr[2].get<float>());
                        box.max = Vector3(maxArr[0].get<float>(), maxArr[1].get<float>(), maxArr[2].get<float>());
                        aabbs.push_back(box);
                    }
                }
            }
            if (!aabbs.empty()) {
                collision->SetLocalAABBs(std::move(aabbs));
                Logger::Debug("[SceneSerializer] Loaded {} AABBs for {}", 
                    collision->GetLocalAABBs().size(),
                    gameObject.GetName());
            }
        }
    }
    else if (type == "LuaScriptComponent") {
        auto* luaScript = gameObject.AddComponent<LuaScriptComponent>();
        if (json.contains("scriptPath")) {
            std::string scriptPath = json["scriptPath"].get<std::string>();
            if (!scriptPath.empty()) {
                luaScript->SetScriptPath(scriptPath);
            }
        }
        // プロパティを復元
        if (json.contains("properties") && json["properties"].is_array()) {
            for (const auto& propJson : json["properties"]) {
                if (!propJson.contains("name") || !propJson.contains("type") || !propJson.contains("value")) {
                    continue;
                }
                std::string name = propJson["name"].get<std::string>();
                std::string propType = propJson["type"].get<std::string>();
                
                if (propType == "bool") {
                    luaScript->SetProperty(name, propJson["value"].get<bool>());
                } else if (propType == "int") {
                    luaScript->SetProperty(name, propJson["value"].get<int32>());
                } else if (propType == "float") {
                    luaScript->SetProperty(name, propJson["value"].get<float>());
                } else if (propType == "string") {
                    luaScript->SetProperty(name, propJson["value"].get<std::string>());
                }
            }
        }
    }
    else if (type == "NavAgentComponent") {
        auto* navAgent = gameObject.AddComponent<NavAgentComponent>();
        if (json.contains("speed")) {
            navAgent->SetSpeed(json["speed"].get<float>());
        }
        if (json.contains("angularSpeed")) {
            navAgent->SetAngularSpeed(json["angularSpeed"].get<float>());
        }
        if (json.contains("acceleration")) {
            navAgent->SetAcceleration(json["acceleration"].get<float>());
        }
        if (json.contains("stoppingDistance")) {
            navAgent->SetStoppingDistance(json["stoppingDistance"].get<float>());
        }
        if (json.contains("agentRadius")) {
            navAgent->SetAgentRadius(json["agentRadius"].get<float>());
        }
        if (json.contains("agentHeight")) {
            navAgent->SetAgentHeight(json["agentHeight"].get<float>());
        }
        if (json.contains("useCrowd")) {
            navAgent->SetUseCrowd(json["useCrowd"].get<bool>());
        }
        if (json.contains("waitTime")) {
            navAgent->SetWaitTime(json["waitTime"].get<float>());
        }
    }
}

} // namespace UnoEngine
