#pragma once

#include "../Engine/Core/Application.h"
#include "../Engine/Animation/AnimationSystem.h"
#include "Systems/PlayerSystem.h"

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

    PlayerSystem* GetPlayerSystem() { return GetSystemManager()->GetSystem<PlayerSystem>(); }
    GraphicsDevice* GetGraphicsDevice() { return graphics_.get(); }
    Renderer* GetRenderer() { return renderer_.get(); }
    LightManager* GetLightManager() { return lightManager_.get(); }

protected:
    void OnInit() override;
    void OnRender() override;

};

} // namespace UnoEngine
