#include "GameScene.h"
#include "../Components/Player.h"
#include "../GameApplication.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Core/Logger.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Rendering/SkinnedMeshRenderer.h"
#include "../../Engine/Rendering/Renderer.h"
#include "../../Engine/Rendering/DebugRenderer.h"
#include "../../Engine/Animation/AnimatorComponent.h"
#include "../../Engine/Animation/AnimationSystem.h"
#include "../../Engine/Systems/SystemManager.h"
#include "../../Engine/Resource/ResourceManager.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace UnoEngine {

void GameScene::OnLoad() {
    Logger::Info("[シーン] GameScene 読み込み開始...");

    SetupCamera();
    SetupPlayer();
    SetupLighting();
    SetupAnimatedCharacter();

#ifdef _DEBUG
    auto* app = static_cast<GameApplication*>(GetApplication());
    editorUI_.Initialize(app->GetGraphicsDevice());
    editorUI_.AddConsoleMessage("[シーン] GameScene 読み込み完了");
#endif

    Logger::Info("[シーン] GameScene 読み込み完了");
}

void GameScene::SetupCamera() {
    auto camera = MakeUnique<Camera>();
    camera->SetPosition(Vector3(0.0f, 1.0f, 3.0f));
    camera->SetRotation(Quaternion::LookRotation(Vector3(0.0f, 0.0f, -1.0f).Normalize(), Vector3::UnitY()));
    SetActiveCamera(camera.release());
}

void GameScene::SetupPlayer() {
    player_ = CreateGameObject("Player");
    player_->AddComponent<Player>();
}

void GameScene::SetupLighting() {
    auto* light = CreateGameObject("DirectionalLight");
    auto* lightComp = light->AddComponent<DirectionalLightComponent>();
    lightComp->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
    lightComp->SetColor(Vector3(1.0f, 1.0f, 1.0f));
    lightComp->SetIntensity(1.0f);
    lightComp->UseTransformDirection(false);
}

void GameScene::SetupAnimatedCharacter() {
    auto* app = static_cast<GameApplication*>(GetApplication());
    auto* resourceManager = app->GetResourceManager();

    // Load model via ResourceManager
    resourceManager->BeginUpload();
    
    loadedModelPath_ = "assets/model/testmodel/walk.gltf";
    auto* modelData = resourceManager->LoadSkinnedModel(loadedModelPath_);
    
    resourceManager->EndUpload();

    if (!modelData) {
        Logger::Error("[エラー] モデル読み込み失敗: {}", loadedModelPath_);
        return;
    }

    // Create animated character with components
    animatedCharacter_ = CreateGameObject("AnimatedCharacter");

    // Add SkinnedMeshRenderer component
    auto* renderer = animatedCharacter_->AddComponent<SkinnedMeshRenderer>();
    renderer->SetModel(modelData);

    // Add AnimatorComponent
    auto* animator = animatedCharacter_->AddComponent<AnimatorComponent>();
    
    // Initialize animator with skeleton and animations
    if (modelData->skeleton) {
        animator->Initialize(modelData->skeleton, modelData->animations);
        
        // Play first animation
        if (!modelData->animations.empty()) {
            std::string animName = modelData->animations[0]->GetName();
            if (animName.empty()) {
                animName = "Animation_0";
            }
            animator->Play(animName, true);
            Logger::Info("[アニメーション] '{}' 再生開始", animName);
        }
    }

#ifdef _DEBUG
    editorUI_.AddConsoleMessage("[Scene] Skinned model loaded: " + loadedModelPath_);
#endif
}

void GameScene::OnUpdate(float deltaTime) {
    // Call base class OnUpdate (processes Start() and updates GameObjects)
    Scene::OnUpdate(deltaTime);

#ifdef _DEBUG
    auto* app = static_cast<GameApplication*>(GetApplication());
    uint32 gameW, gameH, sceneW, sceneH;
    editorUI_.GetDesiredViewportSizes(gameW, gameH, sceneW, sceneH);
    editorUI_.GetGameViewTexture()->Resize(app->GetGraphicsDevice(), gameW, gameH);
    editorUI_.GetSceneViewTexture()->Resize(app->GetGraphicsDevice(), sceneW, sceneH);
#endif

    // Player camera control
    Camera* camera = GetActiveCamera();
    if (camera && player_ && input_) {
        auto* app = static_cast<GameApplication*>(GetApplication());
        auto* cameraSystem = app->GetCameraSystem();
        if (cameraSystem) {
            auto* playerComp = player_->GetComponent<Player>();
            cameraSystem->Update(camera, playerComp, input_, deltaTime);
        }
    }
}

void GameScene::OnImGui() {
#ifdef _DEBUG
    EditorContext context;
    context.player = player_;
    context.camera = GetActiveCamera();
    context.gameObjects = &GetGameObjects();
    context.fps = ImGui::GetIO().Framerate;
    context.frameTime = 1000.0f / ImGui::GetIO().Framerate;

    if (!loadedModelPath_.empty()) {
        context.loadedModels.push_back(loadedModelPath_);
    }
    context.currentSceneName = "GameScene";

    // Get DebugRenderer and AnimationSystem from GameApplication
    auto* app = static_cast<GameApplication*>(GetApplication());
    if (app) {
        if (app->GetRenderer()) {
            context.debugRenderer = app->GetRenderer()->GetDebugRenderer();
        }
        if (app->GetSystemManager()) {
            context.animationSystem = app->GetSystemManager()->GetSystem<AnimationSystem>();
        }
    }

    editorUI_.Render(context);
#endif
}

void GameScene::OnRender(RenderView& view) {
    Camera* camera = GetActiveCamera();
    if (!camera) return;

    view.camera = camera;
    view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
    view.viewName = "MainView";
    // Skinned mesh rendering is handled by RenderSystem in GameApplication
}

} // namespace UnoEngine
