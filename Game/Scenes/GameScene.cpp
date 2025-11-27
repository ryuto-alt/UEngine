#include "GameScene.h"
#include "../Components/Player.h"
#include "../GameApplication.h"
#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Graphics/MeshRenderer.h"
#include "../../Engine/Graphics/DirectionalLightComponent.h"
#include "../../Engine/Math/Math.h"
#include "../../Engine/Rendering/SkinnedRenderItem.h"
#include "../../Engine/Animation/AnimatorComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace UnoEngine {

void GameScene::OnLoad() {
    SetActiveCamera(nullptr);

    // Camera setup - gltfモデル用に調整
    auto camera = MakeUnique<Camera>();
    camera->SetPosition(Vector3(0.0f, 1.0f, 3.0f));  // 近くに配置
    camera->SetRotation(Quaternion::LookRotation(Vector3(0.0f, 0.0f, -1.0f).Normalize(), Vector3::UnitY()));
    SetActiveCamera(camera.release());

    // Player setup (カメラ操作用)
    player_ = CreateGameObject("Player");
    player_->AddComponent<Player>();
    auto* app = static_cast<GameApplication*>(GetApplication());

    // Light setup
    auto* light = CreateGameObject("DirectionalLight");
    auto* lightComp = light->AddComponent<DirectionalLightComponent>();
    lightComp->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
    lightComp->SetColor(Vector3(1.0f, 1.0f, 1.0f));
    lightComp->SetIntensity(1.0f);
    lightComp->UseTransformDirection(false);

    // リソースアップロード開始（コマンドリストを開く）
    auto* graphics = app->GetGraphicsDevice();
    graphics->BeginResourceUpload();

    // Load skinned model (walk.gltf)
    auto* commandList = graphics->GetCommandList();
    skinnedModel_ = SkinnedModelImporter::Load(graphics, commandList, "assets/model/testmodel/walk.gltf");

    // Create animated character GameObject
    animatedCharacter_ = CreateGameObject("AnimatedCharacter");
    auto* animatorComp = animatedCharacter_->AddComponent<AnimatorComponent>();
    
    // Initialize animator with skeleton and animations
    if (skinnedModel_.skeleton) {
        animatorComp->Initialize(skinnedModel_.skeleton, skinnedModel_.animations);
        
        // Play first animation if available
        if (!skinnedModel_.animations.empty()) {
            std::string animName = skinnedModel_.animations[0]->GetName();
            if (animName.empty()) {
                animName = "Animation_0";
            }
            animatorComp->Play(animName, true);
        }
    }

#ifdef _DEBUG
    // EditorUI初期化 (Debug builds only)
    editorUI_.Initialize(graphics);
    editorUI_.AddConsoleMessage("[Scene] GameScene loaded successfully");
    if (!skinnedModel_.meshes.empty()) {
        editorUI_.AddConsoleMessage("[Scene] Skinned model loaded: walk.gltf");
    }
#endif

    // リソースアップロード完了（GPUにコマンドを送信して待機）
    graphics->EndResourceUpload();
}

void GameScene::OnUpdate(float deltaTime) {
#ifdef _DEBUG
    // 前のフレームで記録したサイズでRenderTextureをリサイズ（描画前に実行）
    auto* app = static_cast<GameApplication*>(GetApplication());
    uint32 gameW, gameH, sceneW, sceneH;
    editorUI_.GetDesiredViewportSizes(gameW, gameH, sceneW, sceneH);
    editorUI_.GetGameViewTexture()->Resize(app->GetGraphicsDevice(), gameW, gameH);
    editorUI_.GetSceneViewTexture()->Resize(app->GetGraphicsDevice(), sceneW, sceneH);
#endif

    // PlayerSystemでカメラ移動を処理
    Camera* camera = GetActiveCamera();
    if (camera && player_ && input_) {
        auto* app = static_cast<GameApplication*>(GetApplication());
        auto* playerSystem = app->GetPlayerSystem();
        if (playerSystem) {
            auto* playerComp = player_->GetComponent<Player>();
            playerSystem->Update(camera, playerComp, input_, deltaTime);
        }
    }
}

void GameScene::OnImGui() {
#ifdef _DEBUG
    // EditorContextを構築
    EditorContext context;
    context.player = player_;
    context.camera = GetActiveCamera();
    context.gameObjects = &GetGameObjects();
    context.fps = ImGui::GetIO().Framerate;
    context.frameTime = 1000.0f / ImGui::GetIO().Framerate;

    // ロードされたアセット情報を設定
    if (!skinnedModel_.meshes.empty()) {
        context.loadedModels.push_back("walk.gltf");
    }
    if (!skinnedModel_.meshes.empty() && skinnedModel_.meshes[0].GetMaterial()) {
        context.loadedTextures.push_back("white.png");
    }
    context.currentSceneName = "GameScene";

    // EditorUIに描画を委譲
    editorUI_.Render(context);
#endif
}

void GameScene::OnRender(RenderView& view) {
    Camera* camera = GetActiveCamera();
    if (!camera) return;

    view.camera = camera;
    view.layerMask = Layers::DEFAULT | Layers::PLAYER | Layers::ENEMY;
    view.viewName = "MainView";
    // スキンメッシュの描画はGameApplication::OnRenderで行う
}

std::vector<SkinnedRenderItem> GameScene::GetSkinnedRenderItems() const {
    std::vector<SkinnedRenderItem> items;

    if (!skinnedModel_.meshes.empty() && animatedCharacter_) {
        auto* animatorComp = animatedCharacter_->GetComponent<AnimatorComponent>();
        
        for (const auto& mesh : skinnedModel_.meshes) {
            SkinnedRenderItem item;
            item.mesh = const_cast<SkinnedMesh*>(&mesh);
            
            // Get base world matrix with X-axis rotation to stand the model up
            // glTF models are often lying down, rotate -90 degrees around X to stand up
            Matrix4x4 standUpRotation = Matrix4x4::RotationX(Math::PI / 2.0f);
            item.worldMatrix = standUpRotation * animatedCharacter_->GetTransform().GetWorldMatrix();
            
            item.material = const_cast<Material*>(mesh.GetMaterial());
            
            // Get bone matrices from AnimatorComponent
            if (animatorComp) {
                item.boneMatrixPairs = const_cast<std::vector<BoneMatrixPair>*>(&animatorComp->GetBoneMatrixPairs());
            }
            
            items.push_back(item);
        }
    }

    return items;
}

} // namespace UnoEngine
