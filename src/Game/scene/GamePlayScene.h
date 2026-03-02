#pragma once
#include "IScene.h"
#include "ECS/World.h"
#include "ECS/Entity.h"
#include "Scene/SceneConfigurator.h"
#include "../Utils/JsonLoader.h"
#include <memory>
#include <vector>
#include <string>

namespace ECS {
class RenderSystem;
class UIRenderSystem;
}

class GamePlayScene : public IScene {
public:
    // Continue時に保持するゲーム進行データ
    struct GameProgress {
        bool isResuming = false;
        std::vector<bool> collectedOrbs;
    };
    static GameProgress s_gameProgress;

    GamePlayScene();
    ~GamePlayScene() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    void RegisterSystems();

    ECS::World* m_world = nullptr;

    // Entity handles
    ECS::Entity m_playerEntity;
    ECS::Entity m_enemyEntity;
    ECS::Entity m_gameStateEntity;
    std::vector<ECS::Entity> m_orbEntities;
    std::vector<ECS::Entity> m_sceneObjectEntities;

    // Render systems (called in Draw, not in UpdateSystems)
    std::unique_ptr<ECS::RenderSystem> m_renderSystem;
    std::unique_ptr<ECS::UIRenderSystem> m_uiRenderSystem;

    // Scene configuration data (kept for JSON loading)
    SceneData m_sceneData;

    // NavMesh debug (kept for ImGui)
    std::vector<std::string> m_navMeshLogs;
    bool m_showNavMeshDebug = false;
    void AddNavMeshLog(const std::string& message);
    void ClearNavMeshLogs();
};
