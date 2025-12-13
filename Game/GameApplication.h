#pragma once

#include "../Engine/Core/Application.h"
#include "../Engine/Animation/AnimationSystem.h"
#include "../Engine/Audio/AudioSystem.h"
#include "../Engine/Systems/CollisionSystem.h"
#include "../Engine/Resource/ResourceManager.h"
#include "../Engine/PostProcess/PostProcessType.h"
#include "../Engine/PostProcess/PostProcessManager.h"
#include "../Engine/Graphics/RenderTexture.h"
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
    CollisionSystem* GetCollisionSystem() { return GetSystemManager()->GetSystem<CollisionSystem>(); }
    GraphicsDevice* GetGraphicsDevice() { return graphics_.get(); }
    Renderer* GetRenderer() { return renderer_.get(); }
    LightManager* GetLightManager() { return lightManager_.get(); }
    ResourceManager* GetResourceManager() { return resourceManager_.get(); }

protected:
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

private:
    std::unique_ptr<ResourceManager> resourceManager_;

    // Release用ポストプロセス
    std::unique_ptr<PostProcessManager> m_postProcessManager;
    std::unique_ptr<RenderTexture> m_gameRenderTarget;
    std::unique_ptr<RenderTexture> m_postProcessOutput;

    // Release用マウスロック
    bool m_mouseLocked = false;
    int m_mouseLockX = 0;
    int m_mouseLockY = 0;
    bool m_tabWasPressed = false;
    bool m_clickWasPressed = false;
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 0.0f;
};

} // namespace UnoEngine
