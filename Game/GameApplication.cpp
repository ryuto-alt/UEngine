#include "pch.h"
#include "GameApplication.h"
#include "../Engine/Core/Scene.h"
#include "../Engine/Core/CameraComponent.h"
#include "../Engine/Resource/ResourceLoader.h"
#include "../Engine/Rendering/RenderSystem.h"
#include "../Engine/Rendering/SkinnedRenderItem.h"
#include "../Engine/Audio/AudioSystem.h"
#include "../Engine/Systems/CollisionSystem.h"
#include "../Engine/Core/Logger.h"
#include <Windows.h>
#include <cmath>

namespace UnoEngine {

void GameApplication::OnInit() {
    // Initialize ResourceManager
    resourceManager_ = std::make_unique<ResourceManager>(graphics_.get());
    Logger::Info("[初期化] ResourceManager 準備完了");

    // Register systems
    GetSystemManager()->RegisterSystem<AnimationSystem>();
    GetSystemManager()->RegisterSystem<CameraSystem>();
    GetSystemManager()->RegisterSystem<AudioSystem>();
    GetSystemManager()->RegisterSystem<CollisionSystem>();
    Logger::Info("[初期化] システム登録完了 (Animation, Camera, Audio, Collision)");

#ifndef _DEBUG
    // Release用ポストプロセス初期化
    uint32 width = GetWindow()->GetWidth();
    uint32 height = GetWindow()->GetHeight();

    m_postProcessManager = std::make_unique<PostProcessManager>();
    m_postProcessManager->Initialize(graphics_.get(), width, height);

    m_gameRenderTarget = std::make_unique<RenderTexture>();
    m_gameRenderTarget->Create(graphics_.get(), width, height, 100);

    m_postProcessOutput = std::make_unique<RenderTexture>();
    m_postProcessOutput->Create(graphics_.get(), width, height, 101);

    Logger::Info("[初期化] Release用ポストプロセスシステム準備完了");
#endif
}

Mesh* GameApplication::LoadMesh(const std::string& path) {
    return ResourceLoader::LoadMesh(path);
}

Material* GameApplication::LoadMaterial(const std::string& name) {
    return ResourceLoader::LoadMaterial(name);
}

void GameApplication::OnUpdate(float deltaTime) {
#ifndef _DEBUG
    Scene* scene = GetSceneManager()->GetActiveScene();
    if (!scene) {
        Logger::Warning("[GameApplication] No active scene");
        return;
    }

    Camera* camera = scene->GetActiveCamera();
    if (!camera) {
        Logger::Warning("[GameApplication] No active camera");
        return;
    }

    // TABでマウスロック解除
    bool tabDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
    if (tabDown && m_mouseLocked && !m_tabWasPressed) {
        m_mouseLocked = false;
        while (ShowCursor(TRUE) < 0);
        Logger::Info("[GameApplication] Mouse unlocked");
    }
    m_tabWasPressed = tabDown;

    // 左クリックでマウスロック開始
    bool clickDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (clickDown && !m_mouseLocked && !m_clickWasPressed) {
        m_mouseLocked = true;
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        m_mouseLockX = cursorPos.x;
        m_mouseLockY = cursorPos.y;
        while (ShowCursor(FALSE) >= 0);

        // カメラの向きからyaw/pitchを初期化
        Vector3 forward = camera->GetForward();
        m_cameraYaw = std::atan2(forward.GetX(), forward.GetZ());
        m_cameraPitch = std::asin(-forward.GetY());
        Logger::Info("[GameApplication] Mouse locked at ({}, {})", m_mouseLockX, m_mouseLockY);
    }
    m_clickWasPressed = clickDown;

    // マウスロック中の視点操作
    if (m_mouseLocked) {
        POINT currentPos;
        GetCursorPos(&currentPos);

        float deltaX = static_cast<float>(currentPos.x - m_mouseLockX);
        float deltaY = static_cast<float>(currentPos.y - m_mouseLockY);

        if (deltaX != 0.0f || deltaY != 0.0f) {
            SetCursorPos(m_mouseLockX, m_mouseLockY);

            constexpr float sensitivity = 0.003f;
            m_cameraYaw += deltaX * sensitivity;
            m_cameraPitch += deltaY * sensitivity;

            // Pitch制限
            constexpr float maxPitch = 1.5f;
            if (m_cameraPitch > maxPitch) m_cameraPitch = maxPitch;
            if (m_cameraPitch < -maxPitch) m_cameraPitch = -maxPitch;

            // 回転を適用
            Quaternion rotY = Quaternion::RotationAxis(Vector3::UnitY(), m_cameraYaw);
            Quaternion rotX = Quaternion::RotationAxis(Vector3::UnitX(), m_cameraPitch);
            camera->SetRotation(rotY * rotX);
        }

        // WASD移動
        Vector3 movement(0.0f, 0.0f, 0.0f);
        constexpr float moveSpeed = 5.0f;
        Vector3 fwd = camera->GetForward();
        Vector3 right = camera->GetRight();

        if (GetAsyncKeyState('W') & 0x8000) movement = movement + fwd;
        if (GetAsyncKeyState('S') & 0x8000) movement = movement - fwd;
        if (GetAsyncKeyState('A') & 0x8000) movement = movement - right;
        if (GetAsyncKeyState('D') & 0x8000) movement = movement + right;
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) movement = movement + Vector3::UnitY();
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) movement = movement - Vector3::UnitY();

        if (movement.Length() > 0.001f) {
            movement = movement.Normalize() * moveSpeed * deltaTime;
            camera->SetPosition(camera->GetPosition() + movement);
        }
    }
#endif
}

void GameApplication::OnRender() {
    graphics_->BeginFrame();
    renderer_->BeginFrame();  // ダイナミックバッファをリセット

    Scene* scene = GetSceneManager()->GetActiveScene();
    if (scene) {
        RenderView view;
        scene->OnRender(view);

        // Main Cameraを持つGameObjectからCameraComponentを探す
        CameraComponent* camComp = nullptr;
        for (auto& obj : scene->GetGameObjects()) {
            auto* cc = obj->GetComponent<CameraComponent>();
            if (cc && cc->IsMain()) {
                camComp = cc;
                break;
            }
        }

        // 一人称視点でターゲットモデルを除外する設定
        if (camComp) {
            view.excludeFromFirstPerson = camComp->GetFirstPersonExcludeTarget();
        }

        // Collect render items via RenderSystem
        auto items = renderSystem_->CollectRenderables(scene, view);
        auto skinnedItems = renderSystem_->CollectSkinnedRenderables(scene, view);
        
        static bool loggedOnce = false;
        if (!loggedOnce) {
            Logger::Info("[描画] スキンメッシュ {}個 収集完了", skinnedItems.size());
            loggedOnce = true;
        }

#ifdef _DEBUG
        auto* editorUI = scene->GetEditorUI();
        if (editorUI) {
            auto* debugRenderer = renderer_->GetDebugRenderer();

            // Scene View用カメラを取得（Main Cameraとは完全に独立したEditorCamera）
            Camera* sceneCamera = editorUI->GetSceneViewCamera();

            // デバッグ: カメラが異なることを確認
            if (sceneCamera == view.camera) {
                Logger::Warning("[描画] SceneCameraとMainCameraが同じです！");
            }

            // Game Viewに描画（Main Cameraを使用）
            auto* gameViewTex = editorUI->GetGameViewTexture();
            if (gameViewTex && gameViewTex->GetResource() && view.camera) {
                renderer_->DrawToTexture(
                    gameViewTex->GetResource(),
                    gameViewTex->GetRTVHandle(),
                    gameViewTex->GetDSVHandle(),
                    view,  // Main Camera
                    items,
                    lightManager_.get(),
                    skinnedItems,
                    false  // デバッグ描画無効
                );

                // ポストプロセス設定を取得して適用
                if (camComp && camComp->IsPostProcessEnabled() && 
                    !camComp->GetPostProcessEffects().empty()) {
                    auto* postProcessMgr = editorUI->GetPostProcessManager();
                    auto* postProcessOutput = editorUI->GetPostProcessOutputTexture();
                    if (postProcessMgr && postProcessOutput) {
                        postProcessMgr->SetEffectChain(camComp->GetPostProcessEffects());
                        postProcessMgr->Apply(graphics_.get(), gameViewTex, postProcessOutput);
                    }
                } else {
                    auto* postProcessMgr = editorUI->GetPostProcessManager();
                    if (postProcessMgr) {
                        postProcessMgr->ClearEffects();
                    }
                }
            }

            // Scene Viewに描画（EditorCameraを使用）
            auto* sceneViewTex = editorUI->GetSceneViewTexture();
            if (sceneViewTex && sceneViewTex->GetResource() && sceneCamera) {
                // デバッグ描画の準備
                if (debugRenderer) {
                    debugRenderer->BeginFrame();
                    editorUI->PrepareSceneViewGizmos(debugRenderer);
                }

                // Scene View用のRenderViewを作成
                RenderView sceneView;
                sceneView.camera = sceneCamera;  // EditorCamera（sceneViewCamera_）
                sceneView.layerMask = view.layerMask;
                sceneView.viewName = "SceneView";

                renderer_->DrawToTexture(
                    sceneViewTex->GetResource(),
                    sceneViewTex->GetRTVHandle(),
                    sceneViewTex->GetDSVHandle(),
                    sceneView,
                    items,
                    lightManager_.get(),
                    skinnedItems,
                    true  // デバッグ描画有効
                );
            }

            // メインウィンドウのレンダーターゲットを再設定
            graphics_->SetBackBufferAsRenderTarget();

            // UIのみ描画
            renderer_->RenderUIOnly(scene);
        }
#else
        // Release: レンダーテクスチャに描画してポストプロセス適用
        if (m_gameRenderTarget && m_gameRenderTarget->GetResource()) {
            renderer_->DrawToTexture(
                m_gameRenderTarget->GetResource(),
                m_gameRenderTarget->GetRTVHandle(),
                m_gameRenderTarget->GetDSVHandle(),
                view,
                items,
                lightManager_.get(),
                skinnedItems,
                false  // デバッグ描画無効
            );

            // ポストプロセス適用
            bool hasPostProcess = camComp && camComp->IsPostProcessEnabled() &&
                                  !camComp->GetPostProcessEffects().empty();

            if (hasPostProcess && m_postProcessManager && m_postProcessOutput) {
                // CameraComponentのパラメータをPostProcessManagerに同期
                if (auto* vignette = m_postProcessManager->GetVignette()) {
                    vignette->SetParams(camComp->GetVignetteParams());
                }
                if (auto* fisheye = m_postProcessManager->GetFisheye()) {
                    fisheye->SetParams(camComp->GetFisheyeParams());
                }
                if (auto* grayscale = m_postProcessManager->GetGrayscale()) {
                    grayscale->SetParams(camComp->GetGrayscaleParams());
                }
                if (auto* ps1 = m_postProcessManager->GetPS1()) {
                    ps1->SetParams(camComp->GetPS1Params());
                }

                m_postProcessManager->SetEffectChain(camComp->GetPostProcessEffects());
                m_postProcessManager->Apply(graphics_.get(), m_gameRenderTarget.get(), m_postProcessOutput.get());
                m_postProcessManager->BlitToBackBuffer(graphics_.get(), m_postProcessOutput.get());
            } else {
                // ポストプロセスなしの場合は直接バックバッファにコピー
                m_postProcessManager->BlitToBackBuffer(graphics_.get(), m_gameRenderTarget.get());
            }
        } else {
            // フォールバック: 直接描画
            renderer_->Draw(view, items, lightManager_.get(), scene);
            if (!skinnedItems.empty()) {
                renderer_->DrawSkinnedMeshes(view, skinnedItems, lightManager_.get());
            }
        }
#endif
    }

    graphics_->EndFrame();
    graphics_->Present();
}

} // namespace UnoEngine
