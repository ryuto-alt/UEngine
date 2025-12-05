#include "pch.h"
#include "Scene.h"
#include "Component.h"
#include "Logger.h"
#include "CameraComponent.h"
#include "../Scene/SceneSerializer.h"
#include "../Rendering/SkinnedMeshRenderer.h"
#include "../Graphics/MeshRenderer.h"
#include "../Animation/AnimatorComponent.h"
#include "../Animation/AnimationSystem.h"
#include "../Graphics/DirectionalLightComponent.h"
#include "../Rendering/RenderView.h"
#include "../Rendering/Renderer.h"
#include "../Rendering/DebugRenderer.h"
#include "../Systems/SystemManager.h"
#include "../Resource/ResourceManager.h"
#include "../Resource/StaticModelImporter.h"
#include "../../Game/GameApplication.h"
#include <algorithm>
#include <fstream>

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace UnoEngine {

Scene::Scene(const std::string& name)
    : name_(name) {
}

void Scene::OnLoad() {
    Logger::Info("[シーン] Scene 読み込み開始...");

    // シーンファイルが存在するか確認
    const std::string sceneFilePath = "assets/scenes/default_scene.json";
    std::ifstream sceneFile(sceneFilePath);
    bool sceneFileExists = sceneFile.good();
    sceneFile.close();

    if (sceneFileExists) {
        LoadSceneFromFile(sceneFilePath);
    } else {
        // シーンファイルがない場合はデフォルトカメラを作成
        Logger::Info("[シーン] シーンファイルが見つかりません。デフォルトカメラを作成します。");
        SetupDefaultCamera();
    }

#ifdef _DEBUG
    auto* app = static_cast<GameApplication*>(GetApplication());
    editorUI_.Initialize(app->GetGraphicsDevice());
    editorUI_.SetGameObjects(&GetGameObjects());
    editorUI_.SetResourceManager(app->GetResourceManager());
    editorUI_.SetScene(this);
    editorUI_.SetAudioSystem(app->GetAudioSystem());

    // Game Camera（Main Camera）を設定
    editorUI_.SetGameCamera(GetActiveCamera());

    if (sceneFileExists) {
        editorUI_.AddConsoleMessage("[シーン] 保存されたシーンをロード: " + sceneFilePath);
    }

    // D&Dでキューに入れられたモデルを読み込む
    editorUI_.ProcessPendingLoads();

    editorUI_.AddConsoleMessage("[シーン] Scene 読み込み完了");
#endif

    Logger::Info("[シーン] Scene 読み込み完了");
}

void Scene::LoadSceneFromFile(const std::string& filepath) {
    Logger::Info("[シーン] 保存されたシーンをロード: {}", filepath);

    if (!SceneSerializer::LoadScene(filepath, GetGameObjects())) {
        Logger::Warning("[シーン] シーンのロードに失敗しました。デフォルトカメラを作成します。");
        SetupDefaultCamera();
        return;
    }

    // ロード成功時はモデルを再ロードしてポインタを更新
    auto* app = static_cast<GameApplication*>(GetApplication());
    auto* resourceManager = app->GetResourceManager();

    bool foundMainCamera = false;

    // 各モデルを個別にロード
    for (auto& obj : GetGameObjects()) {
        // CameraComponentを持つオブジェクトを検出
        if (auto* cameraComp = obj->GetComponent<CameraComponent>()) {
            if (cameraComp->IsMain() || !foundMainCamera) {
                mainCamera_ = obj.get();
                obj->SetDeletable(false);
                cameraComp->SetMain(true);
                SetActiveCamera(cameraComp->GetCamera());
                foundMainCamera = true;
            }
        }

        // SkinnedMeshRendererを持つオブジェクトのモデルを再ロード
        auto* skinnedRenderer = obj->GetComponent<SkinnedMeshRenderer>();
        if (skinnedRenderer) {
            std::string modelPath = skinnedRenderer->GetModelPath();
            if (!modelPath.empty()) {
                resourceManager->BeginUpload();
                auto* modelData = resourceManager->LoadSkinnedModel(modelPath);
                resourceManager->EndUpload();

                if (modelData) {
                    skinnedRenderer->SetModel(modelData);

                    // Animatorを再初期化
                    auto* animator = obj->GetComponent<AnimatorComponent>();
                    if (!animator) {
                        animator = obj->AddComponent<AnimatorComponent>();
                    }

                    if (modelData->skeleton) {
                        animator->Initialize(modelData->skeleton, modelData->animations);
                        if (!modelData->animations.empty()) {
                            animator->Play(modelData->animations[0]->GetName(), true);
                        }
                    }

                    Logger::Info("[シーン] スキンモデル再ロード完了: {}", modelPath);
                } else {
                    Logger::Warning("[シーン] スキンモデル再ロード失敗: {}", modelPath);
                }
            }
        }

        // MeshRendererを持つオブジェクトのモデルを再ロード（静的モデル）
        auto* meshRenderer = obj->GetComponent<MeshRenderer>();
        if (meshRenderer) {
            std::string modelPath = meshRenderer->GetModelPath();
            if (!modelPath.empty()) {
                resourceManager->BeginUpload();
                auto* modelData = resourceManager->LoadStaticModel(modelPath);
                resourceManager->EndUpload();

                if (modelData && !modelData->meshes.empty()) {
                    meshRenderer->SetMesh(&modelData->meshes[0]);
                    if (modelData->meshes[0].HasMaterial()) {
                        meshRenderer->SetMaterial(const_cast<Material*>(modelData->meshes[0].GetMaterial()));
                    }
                    Logger::Info("[シーン] 静的モデル再ロード完了: {}", modelPath);
                } else {
                    Logger::Warning("[シーン] 静的モデル再ロード失敗: {}", modelPath);
                }
            }
        }
    }

    // Main Cameraがシーンに存在しない場合は作成
    if (!foundMainCamera) {
        SetupDefaultCamera();
    }
}

void Scene::SetupDefaultCamera() {
    mainCamera_ = CreateGameObject("Main Camera");
    mainCamera_->SetDeletable(false);

    mainCamera_->GetTransform().SetLocalPosition(Vector3(0.0f, 1.0f, -3.0f));

    auto* cameraComp = mainCamera_->AddComponent<CameraComponent>();
    cameraComp->SetMain(true);
    cameraComp->SetPerspective(60.0f * 0.0174533f, 16.0f / 9.0f, 0.1f, 1000.0f);

    SetActiveCamera(cameraComp->GetCamera());
    SetActiveCameraComponent(cameraComp);
}

void Scene::OnUpdate(float deltaTime) {
#ifdef _DEBUG
    auto* app = static_cast<GameApplication*>(GetApplication());

    // D&Dでキューに入れられたモデルを即座にロード
    editorUI_.ProcessPendingLoads();
#endif

    // Process pending Start() calls before Update
    ProcessPendingStarts();

    // Update all game objects
    for (auto& obj : gameObjects_) {
        obj->OnUpdate(deltaTime);
    }

    // Destroy pending objects
    if (!pendingDestroy_.empty()) {
        for (GameObject* obj : pendingDestroy_) {
            gameObjects_.erase(
                std::remove_if(gameObjects_.begin(), gameObjects_.end(),
                    [obj](const std::unique_ptr<GameObject>& go) {
                        return go.get() == obj;
                    }),
                gameObjects_.end()
            );
        }
        pendingDestroy_.clear();
    }

#ifdef _DEBUG
    uint32 gameW, gameH, sceneW, sceneH;
    editorUI_.GetDesiredViewportSizes(gameW, gameH, sceneW, sceneH);
    editorUI_.GetGameViewTexture()->Resize(app->GetGraphicsDevice(), gameW, gameH);
    editorUI_.GetSceneViewTexture()->Resize(app->GetGraphicsDevice(), sceneW, sceneH);
#endif
}

void Scene::OnRender(RenderView& view) {
    Camera* camera = GetActiveCamera();
    if (!camera) return;

    view.camera = camera;
    view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
    view.viewName = "MainView";
}

void Scene::OnImGui() {
#ifdef _DEBUG
    EditorContext context;
    context.camera = GetActiveCamera();
    context.gameObjects = &GetGameObjects();
    context.fps = ImGui::GetIO().Framerate;
    context.frameTime = 1000.0f / ImGui::GetIO().Framerate;
    context.currentSceneName = name_;

    auto* app = static_cast<GameApplication*>(GetApplication());
    if (app) {
        if (app->GetRenderer()) {
            context.debugRenderer = app->GetRenderer()->GetDebugRenderer();
        }
        if (app->GetSystemManager()) {
            context.animationSystem = app->GetSystemManager()->GetSystem<AnimationSystem>();
        }
        // パーティクルエディターを設定
        editorUI_.SetParticleEditor(app->GetParticleEditor());
    }

    editorUI_.Render(context);
#endif
}

GameObject* Scene::CreateGameObject(const std::string& name) {
    auto obj = std::make_unique<GameObject>(name);
    GameObject* ptr = obj.get();
    gameObjects_.push_back(std::move(obj));
    return ptr;
}

void Scene::DestroyGameObject(GameObject* obj) {
    pendingDestroy_.push_back(obj);
}

void Scene::ProcessPendingStarts() {
    // Call Start() on components that have been Awake'd but not Started
    for (auto& obj : gameObjects_) {
        if (!obj->IsActive()) continue;

        for (auto& comp : obj->GetComponents()) {
            if (comp->IsAwakeCalled() && !comp->HasStarted() && comp->IsEnabled()) {
                comp->Start();
                comp->MarkStarted();
            }
        }
    }
}

void Scene::StartGameObject(GameObject* obj) {
    if (!obj || !obj->IsActive()) return;

    for (auto& comp : obj->GetComponents()) {
        if (comp->IsAwakeCalled() && !comp->HasStarted() && comp->IsEnabled()) {
            comp->Start();
            comp->MarkStarted();
        }
    }
}

} // namespace UnoEngine
