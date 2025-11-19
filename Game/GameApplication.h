#pragma once

#include "../Engine/Core/Application.h"
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

    PlayerSystem& GetPlayerSystem() { return playerSystem_; }
    GraphicsDevice* GetGraphicsDevice() { return graphics_.get(); }

protected:
    void OnInit() override;
    void OnRender() override;

private:
    PlayerSystem playerSystem_;
};

} // namespace UnoEngine
