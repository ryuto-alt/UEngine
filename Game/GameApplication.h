#pragma once

#include "../Engine/Core/Application.h"
#include "../Engine/Animation/AnimationSystem.h"
#include "../Engine/Audio/AudioSystem.h"
#include "../Engine/Resource/ResourceManager.h"
#include "../Engine/PostProcess/PostProcessType.h"
#include "../Engine/Cinematic/CinematicManager.h"
#include "Systems/CameraSystem.h"
#include <memory>

namespace UnoEngine {

class Mesh;
class Material;

class GameApplication : public Application {
public:
    GameApplication() = default;
    explicit GameApplication(const ApplicationConfig& config) : Application(config) {}
    ~GameApplication() override = default;

    // Game-layer resource API
    Mesh* LoadMesh(const std::string& path);
    Material* LoadMaterial(const std::string& name);

    // シネマティック再生中かどうか（外部からカメラ制御を抑制するため）
    bool IsIntroCinematicPlaying() const { return cinematicManager_.IsPlaying(); }
    CinematicManager& GetCinematicManager() { return cinematicManager_; }

    // Accessors
    CameraSystem* GetCameraSystem() { return GetSystemManager()->GetSystem<CameraSystem>(); }
    AudioSystem* GetAudioSystem() { return GetSystemManager()->GetSystem<AudioSystem>(); }
    GraphicsDevice* GetGraphicsDevice() { return graphics_.get(); }
    Renderer* GetRenderer() { return renderer_.get(); }
    LightManager* GetLightManager() { return lightManager_.get(); }
    ResourceManager* GetResourceManager() { return resourceManager_.get(); }

protected:
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
#ifdef WITH_EDITOR
    void OnLoadingPhase() override;
#endif

private:
    std::unique_ptr<ResourceManager> resourceManager_;
    CinematicManager cinematicManager_;
    bool cinematicsLoaded_ = false;

#ifndef WITH_EDITOR
    float escHoldTime_ = 0.0f;
    static constexpr float kEscQuitThreshold = 1.5f;
#endif
};

} // namespace UnoEngine
