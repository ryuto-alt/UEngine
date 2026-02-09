#pragma once
#include "SceneLoader.h"
#include <memory>

class DirectXCommon;
class SrvManager;
class Camera;
class FPSCamera;
class PostProcess;
class LightManager;
class Skybox;
class Object3d;
class UnoEngine;

class SceneConfigurator {
public:
    SceneConfigurator() = default;
    ~SceneConfigurator() = default;

    SceneData LoadSceneFromJSON(const std::string& jsonPath);

    void ApplySceneData(
        const SceneData& sceneData,
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        Camera* camera,
        std::vector<std::unique_ptr<Object3d>>& sceneObjects,
        std::unique_ptr<Skybox>& skybox,
        std::unique_ptr<LightManager>& lightManager,
        std::unique_ptr<FPSCamera>& fpsCamera,
        std::unique_ptr<PostProcess>& postProcess,
        std::unique_ptr<PostProcess>& horrorEffect,
        bool& skyboxEnabled,
        float& fisheyeStrength,
        float& fisheyeRadius
    );

private:
    void ApplyEnvironment(const SceneData& data, bool& skyboxEnabled);
    void ApplyCamera(const SceneData& data, Camera* camera, std::unique_ptr<FPSCamera>& fpsCamera);
    void ApplyPostProcess(const SceneData& data, std::unique_ptr<PostProcess>& postProcess,
                         std::unique_ptr<PostProcess>& horrorEffect,
                         float& fisheyeStrength, float& fisheyeRadius);
    void ApplyObjects(const SceneData& data, std::vector<std::unique_ptr<Object3d>>& sceneObjects, UnoEngine* engine);
    void ApplyAudio(const SceneData& data, UnoEngine* engine);
};
