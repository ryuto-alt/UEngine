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
#include "../../Engine/Audio/AudioSystem.h"
#include "../../Engine/Systems/SystemManager.h"
#include "../../Engine/Resource/ResourceManager.h"
#include "../../Engine/Scene/SceneSerializer.h"
#include <fstream>

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace UnoEngine {

void GameScene::OnLoad() {
    Logger::Info("[シーン] GameScene 読み込み開始...");

    SetupCamera();

    // シーンファイルが存在するか確認
    const std::string sceneFilePath = "assets/scenes/default_scene.json";
    std::ifstream sceneFile(sceneFilePath);
    bool sceneFileExists = sceneFile.good();
    sceneFile.close();

    if (sceneFileExists) {
        // 保存されたシーンをロード
        Logger::Info("[シーン] 保存されたシーンをロード: {}", sceneFilePath);
        if (!SceneSerializer::LoadScene(sceneFilePath, GetGameObjects())) {
            Logger::Warning("[シーン] シーンのロードに失敗しました。デフォルトシーンを作成します。");
            // ロード失敗時はデフォルトシーンを作成
            SetupPlayer();
            SetupLighting();
            SetupAnimatedCharacter();
        } else {
            // ロード成功時はモデルを再ロードしてポインタを更新
            auto* app = static_cast<GameApplication*>(GetApplication());
            auto* resourceManager = app->GetResourceManager();

            // 各モデルを個別にロード（複数モデルを1つのアップロードコンテキストで処理すると描画バグが発生）
            for (auto& obj : GetGameObjects()) {
                if (obj->GetName() == "Player") {
                    player_ = obj.get();
                }

                // SkinnedMeshRendererを持つオブジェクトのモデルを再ロード
                auto* renderer = obj->GetComponent<SkinnedMeshRenderer>();
                if (renderer) {
                    std::string modelPath = renderer->GetModelPath();
                    if (!modelPath.empty()) {
                        // このモデル専用のアップロードコンテキスト
                        resourceManager->BeginUpload();

                        auto* modelData = resourceManager->LoadSkinnedModel(modelPath);

                        resourceManager->EndUpload();

                        if (modelData) {
                            renderer->SetModel(modelData);

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

                            animatedCharacter_ = obj.get();
                            Logger::Info("[シーン] モデル再ロード完了: {}", modelPath);
                        } else {
                            Logger::Warning("[シーン] モデル再ロード失敗: {}", modelPath);
                        }
                    }
                }
            }
        }
    } else {
        // シーンファイルがない場合はデフォルトシーンを作成
        Logger::Info("[シーン] シーンファイルが見つかりません。デフォルトシーンを作成します。");
        SetupPlayer();
        SetupLighting();
        SetupAnimatedCharacter();
    }

#ifdef _DEBUG
    auto* app = static_cast<GameApplication*>(GetApplication());
    editorUI_.Initialize(app->GetGraphicsDevice());
    editorUI_.SetGameObjects(&GetGameObjects());
    editorUI_.SetResourceManager(app->GetResourceManager());
    editorUI_.SetScene(this);
    editorUI_.SetAudioSystem(app->GetAudioSystem());
    if (sceneFileExists) {
        editorUI_.AddConsoleMessage("[シーン] 保存されたシーンをロード: " + sceneFilePath);
    }

    // D&Dでキューに入れられたモデルを読み込む
    editorUI_.ProcessPendingLoads();

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
    // モデルファイル名をGameObject名に使用
    std::string modelName = loadedModelPath_;
    size_t lastSlash = modelName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        modelName = modelName.substr(lastSlash + 1);
    }
    // 拡張子を除去
    size_t lastDot = modelName.find_last_of('.');
    if (lastDot != std::string::npos) {
        modelName = modelName.substr(0, lastDot);
    }
    animatedCharacter_ = CreateGameObject(modelName);

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
#ifdef _DEBUG
    auto* app = static_cast<GameApplication*>(GetApplication());

    // D&Dでキューに入れられたモデルを即座にロード
    editorUI_.ProcessPendingLoads();
#endif

    // Call base class OnUpdate (processes Start() and updates GameObjects)
    Scene::OnUpdate(deltaTime);

#ifdef _DEBUG
    uint32 gameW, gameH, sceneW, sceneH;
    editorUI_.GetDesiredViewportSizes(gameW, gameH, sceneW, sceneH);
    editorUI_.GetGameViewTexture()->Resize(app->GetGraphicsDevice(), gameW, gameH);
    editorUI_.GetSceneViewTexture()->Resize(app->GetGraphicsDevice(), sceneW, sceneH);
#endif

    // Player camera control (Playモードでのみ有効)
#ifdef _DEBUG
    // Playモードで、Game Viewでマウスロック中のみ入力を処理
    if (editorUI_.IsPlaying() && editorUI_.IsGameViewMouseLocked()) {
#endif
        Camera* camera = GetActiveCamera();
        if (camera && player_ && input_) {
            auto* app = static_cast<GameApplication*>(GetApplication());
            auto* cameraSystem = app->GetCameraSystem();
            if (cameraSystem) {
                auto* playerComp = player_->GetComponent<Player>();
                cameraSystem->Update(camera, playerComp, input_, deltaTime);
            }
        }
#ifdef _DEBUG
    }
#endif
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
