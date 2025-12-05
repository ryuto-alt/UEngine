#pragma once

#include "../Engine/Core/Application.h"
#include "../Engine/Animation/AnimationSystem.h"
#include "../Engine/Audio/AudioSystem.h"
#include "../Engine/Resource/ResourceManager.h"
#include "../Engine/PostProcess/PostProcessType.h"
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

    // Accessors
    CameraSystem* GetCameraSystem() { return GetSystemManager()->GetSystem<CameraSystem>(); }
    AudioSystem* GetAudioSystem() { return GetSystemManager()->GetSystem<AudioSystem>(); }
    GraphicsDevice* GetGraphicsDevice() { return graphics_.get(); }
    Renderer* GetRenderer() { return renderer_.get(); }
    LightManager* GetLightManager() { return lightManager_.get(); }
    ResourceManager* GetResourceManager() { return resourceManager_.get(); }

protected:
    void OnInit() override;
    void OnRender() override;

private:
    std::unique_ptr<ResourceManager> resourceManager_;
};

} // namespace UnoEngine
