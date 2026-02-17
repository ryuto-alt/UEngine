#include "GamePlayScene.h"
#include "UnoEngine.h"
#include "SceneManager.h"

// ECS
#include "ECS/World.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/RenderComponents.h"
#include "ECS/Components/PlayerComponents.h"
#include "ECS/Components/CameraComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/AudioComponents.h"
#include "ECS/Components/CollisionComponents.h"
#include "ECS/Components/CollectibleComponents.h"
#include "ECS/Components/GameStateComponents.h"
#include "ECS/Components/LightingComponents.h"
#include "ECS/Components/PostProcessComponents.h"

// Systems
#include "Systems/InputSystem.h"
#include "Systems/DebugInputSystem.h"
#include "Systems/PlayerMovementSystem.h"
#include "Systems/GravitySystem.h"
#include "Systems/GroundCollisionSystem.h"
#include "Systems/VelocityIntegrationSystem.h"
#include "Systems/RotationSmoothingSystem.h"
#include "Systems/CollisionResponseSystem.h"
#include "Systems/SoundDetectionSystem.h"
#include "Systems/VisionSystem.h"
#include "Systems/AIBehaviorSystem.h"
#include "Systems/PathfindingSystem.h"
#include "Systems/StuckDetectionSystem.h"
#include "Systems/StealthSystem.h"
#include "Systems/AmbushWarpSystem.h"
#include "Systems/OrbCollectionSystem.h"
#include "Systems/FloatingAnimationSystem.h"
#include "Systems/SpotLightGlowSystem.h"
#include "Systems/JumpscareSystem.h"
#include "Systems/RespawnSystem.h"
#include "Systems/FearEffectSystem.h"
#include "Systems/TutorialSystem.h"
#include "Systems/StealthTutorialSystem.h"
#include "Systems/AnimationSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/CameraShakeSystem.h"
#include "Systems/AudioListenerSystem.h"
#include "Systems/EnemyFootstepAudioSystem.h"
#include "Systems/DetectionSoundSystem.h"
#include "Systems/BarkSoundSystem.h"
#include "Systems/ChaseBGMSystem.h"
#include "Systems/PlayerFootstepSystem.h"
#include "Systems/LightingSystem.h"
#include "Systems/FlashlightSystem.h"
#include "Systems/LightDistributionSystem.h"
#include "Systems/TransformSyncSystem.h"
#include "Systems/CullingSystemECS.h"
#include "Systems/RenderSystem.h"
#include "Systems/UIRenderSystem.h"

// EntityFactory
#include "EntityFactory.h"

#include "GameObject/FPSCamera.h"
#include "GameObject/EnemyAIConfig.h"

// UI
#include "UI/BitmapFont.h"
#include "UI/SubtitleManager.h"
#include "UI/Minimap.h"
#include "UI/MinimapGenerator.h"
#include "UI/SettingsMenu.h"

// Engine
#include "Collision/AABBCollision.h"
#include "NavMesh/NavMesh.h"
#include "LineRenderer.h"

// ECS component types are in namespace ECS
using namespace ECS;

#include <filesystem>
#include <cmath>
#include <string>

#ifdef _DEBUG
#include "imgui.h"
#endif

// static定義
GamePlayScene::GameProgress GamePlayScene::s_gameProgress;

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

void GamePlayScene::Initialize() {
    if (!dxCommon_ || !srvManager_ || !camera_) {
        OutputDebugStringA("GamePlayScene::Initialize - Critical error: Required pointers are null!\n");
        return;
    }

    UnoEngine* engine = UnoEngine::GetInstance();
    m_world = engine->GetECSWorld();

    SceneConfigurator configurator;
    std::vector<std::unique_ptr<Object3d>> sceneObjects;
    std::unique_ptr<Skybox> skybox;
    std::unique_ptr<LightManager> lightManager;
    std::unique_ptr<FPSCamera> fpsCamera;
    std::unique_ptr<PostProcess> postProcess;
    std::unique_ptr<PostProcess> horrorEffect;
    bool skyboxEnabled = false;
    float fisheyeStrength = 2.58f;
    float fisheyeRadius = 1.5f;

    m_sceneData = configurator.LoadSceneFromJSON("Resources/Scenes/gameplay_scene.json");
    configurator.ApplySceneData(
        m_sceneData, dxCommon_, srvManager_, camera_,
        sceneObjects, skybox, lightManager,
        fpsCamera, postProcess, horrorEffect, skyboxEnabled,
        fisheyeStrength, fisheyeRadius
    );

    // NavMesh initialization
    NavMeshManager* navMeshManager = engine->GetNavMgr();
    if (navMeshManager) {
        navMeshManager->SetLogCallback([this](const std::string& message) {
            AddNavMeshLog(message);
        });
    }
    engine->InitNav("externals/navimap/stage.navmesh");
    navMeshManager = engine->GetNavMgr();
    if (!navMeshManager->GetNavMesh() || !navMeshManager->GetNavMesh()->IsValid()) {
        engine->GenNav(sceneObjects, "externals/navimap/stage.navmesh");
    }

    // Generate minimap textures (cached as PNG)
    const std::string mapPng = "Resources/textures/UI/minimap_navmesh.png";
    const std::string mapBounds = "Resources/textures/UI/minimap_bounds.txt";
    const std::string mapFrame = "Resources/textures/UI/minimap_frame.png";
    if (!std::filesystem::exists(mapPng)) {
        navMeshManager = engine->GetNavMgr();
        if (navMeshManager && navMeshManager->GetNavMesh() && navMeshManager->GetNavMesh()->IsValid()) {
            MinimapGenerator::Generate(navMeshManager->GetNavMesh(), mapPng, mapBounds, 512);
        }
    }
    if (!std::filesystem::exists(mapFrame)) {
        // circleRatio = 1/FRAME_SCALE so the opening appears as MAP_SIZE on screen
        MinimapGenerator::GenerateCircularFrame(mapFrame, 512, 1.0f / 1.45f);
    }

    // --- Create ECS Entities ---

    // Player entity
    Vector3 playerPos = m_sceneData.player.position;
    m_playerEntity = EntityFactory::CreatePlayerEntity(*m_world, playerPos, camera_);

    // Load player model directly
    {
        auto playerModel = engine->CreateAnim();
        auto* preloader = ResourcePreloader::GetInstance();
        auto preloaded = preloader->GetPreloadedModel("human_walk");
        if (preloaded) {
            playerModel = std::move(preloaded);
        } else {
            playerModel->LoadFromFile("Resources/Models/human", "walk.gltf");
        }

        // Register named animations
        Animation walkAnim = playerModel->GetAnimationPlayer().GetAnimation();
        playerModel->AddAnimation("walk", walkAnim);
        Animation sneakWalkAnim = engine->LoadAnim("Resources/Models/human", "sneakWalk.gltf");
        playerModel->AddAnimation("sneakWalk", sneakWalkAnim);
        playerModel->ChangeAnimation("walk");
        playerModel->PlayAnimation();

        auto playerObj = engine->CreateObj3();
        playerObj->SetModel(static_cast<Model*>(playerModel.get()));
        playerObj->SetAnimatedModel(playerModel.get());
        playerObj->SetPosition(playerPos);
        playerObj->SetScale({1.0f, 1.0f, 1.0f});
        playerObj->SetRotation({0.0f, 3.14159f, 0.0f});
        playerObj->SetEnableLighting(true);
        playerObj->SetCamera(camera_);
        playerObj->Update();

        auto& anim = m_world->GetComponent<AnimatedModelComponent>(m_playerEntity);
        anim.animatedModel = std::move(playerModel);
        auto& renderer = m_world->GetComponent<MeshRendererComponent>(m_playerEntity);
        renderer.object3d = std::move(playerObj);

        // Sync TransformComponent rotation with Object3d
        auto& playerTransform = m_world->GetComponent<TransformComponent>(m_playerEntity);
        playerTransform.rotation = {0.0f, 3.14159f, 0.0f};

        // Register player AABB collision
        auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
        if (collisionManager && anim.animatedModel) {
            Collision::AABB playerAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(anim.animatedModel.get());
            collisionManager->RegisterObject(renderer.object3d.get(), playerAABB, true, "Player");
        }
    }

    if (fpsCamera) {
        auto& fpsCam = m_world->GetComponent<FPSCameraComponent>(m_playerEntity);
        fpsCam.fpsCamera = std::move(fpsCamera);
    }

    // Audio listener
    auto audioListener = std::make_unique<SpatialAudioListener>();
    audioListener->SetPosition(playerPos);
    auto& listenerComp = m_world->GetComponent<AudioListenerComponent>(m_playerEntity);
    listenerComp.listener = std::move(audioListener);

    // Enemy entity
    if (!m_sceneData.enemies.empty()) {
        Vector3 enemyPos = m_sceneData.enemies[0].position;
        EnemyAIConfig aiConfig;
        m_enemyEntity = EntityFactory::CreateEnemyEntity(*m_world, enemyPos, aiConfig, camera_);

        // Load enemy model directly
        auto enemyModel = engine->CreateAnim();
        enemyModel->LoadFromFile("Resources/Models/Enemy/Enemy_Walk", "Enemy_Walk.gltf");

        // Register named animations
        Animation walkAnim = enemyModel->GetAnimationPlayer().GetAnimation();
        enemyModel->AddAnimation("Walk", walkAnim);
        Animation runAnim = engine->LoadAnim("Resources/Models/Enemy/Enemy_Run", "Enemy_Run.gltf");
        enemyModel->AddAnimation("Run", runAnim);
        Animation jumpscareAnim = engine->LoadAnim("Resources/Models/Enemy/Enemy_Jumpscare", "Enemy_Jumpscare.gltf");
        enemyModel->AddAnimation("Jumpscare", jumpscareAnim);
        float jumpscareDuration = jumpscareAnim.duration;
        enemyModel->ChangeAnimation("Walk");
        enemyModel->PlayAnimation();

        auto enemyObj = engine->CreateObj3();
        enemyObj->SetModel(static_cast<Model*>(enemyModel.get()));
        enemyObj->SetAnimatedModel(enemyModel.get());
        enemyObj->SetPosition(enemyPos);
        enemyObj->SetScale({0.05f, 0.05f, 0.05f});
        enemyObj->SetRotation({0.0f, 3.14159f, 0.0f});
        enemyObj->SetEnableLighting(true);
        enemyObj->SetCamera(camera_);
        enemyObj->Update();

        auto& enemyAnim = m_world->GetComponent<AnimatedModelComponent>(m_enemyEntity);
        enemyAnim.animatedModel = std::move(enemyModel);
        auto& enemyRenderer = m_world->GetComponent<MeshRendererComponent>(m_enemyEntity);
        enemyRenderer.object3d = std::move(enemyObj);

        // Sync TransformComponent scale/rotation with Object3d
        auto& enemyTransform = m_world->GetComponent<TransformComponent>(m_enemyEntity);
        enemyTransform.scale = {0.05f, 0.05f, 0.05f};
        enemyTransform.rotation = {0.0f, 3.14159f, 0.0f};

        // Sync RotationSmoothingComponent with initial rotation
        auto& enemyRot = m_world->GetComponent<RotationSmoothingComponent>(m_enemyEntity);
        enemyRot.currentRotationY = 3.14159f;
        enemyRot.targetRotationY = 3.14159f;

        // Register enemy AABB collision
        {
            auto* collisionManager = Collision::AABBCollisionManager::GetInstance();
            if (collisionManager && enemyAnim.animatedModel) {
                Collision::AABB enemyAABB = Collision::AABBExtractor::ExtractFromAnimatedModel(enemyAnim.animatedModel.get());
                collisionManager->RegisterObject(enemyRenderer.object3d.get(), enemyAABB, true, "Enemy");
            }
        }

#ifdef _DEBUG
        // Initialize debug foot bone line renderer
        if (m_world->HasComponent<EnemyDebugComponent>(m_enemyEntity)) {
            auto& debugComp = m_world->GetComponent<EnemyDebugComponent>(m_enemyEntity);
            debugComp.lineRenderer = std::make_unique<LineRenderer>();
            debugComp.lineRenderer->Initialize(dxCommon_, camera_);
        }
#endif

        // Initialize enemy audio sources
        auto& footstepAudio = m_world->GetComponent<EnemyFootstepAudioComponent>(m_enemyEntity);
        footstepAudio.footstepSource1 = std::make_unique<SpatialAudioSource>();
        footstepAudio.footstepSource1->Initialize("Resources/Audio/Enemy_feet.mp3", enemyPos);
        footstepAudio.footstepSource1->SetVolume(0.6f);
        footstepAudio.footstepSource1->SetMaxDistance(22.0f);
        footstepAudio.footstepSource1->SetMinDistance(1.0f);
        footstepAudio.footstepSource2 = std::make_unique<SpatialAudioSource>();
        footstepAudio.footstepSource2->Initialize("Resources/Audio/Enemy_feet2.mp3", enemyPos);
        footstepAudio.footstepSource2->SetVolume(0.6f);
        footstepAudio.footstepSource2->SetMaxDistance(22.0f);
        footstepAudio.footstepSource2->SetMinDistance(1.0f);

        auto& detection = m_world->GetComponent<DetectionSoundComponent>(m_enemyEntity);
        detection.source = std::make_unique<SpatialAudioSource>();
        detection.source->Initialize("Resources/Audio/enemysound.mp3", enemyPos);
        detection.source->SetVolume(0.55f);
        detection.source->SetMaxDistance(40.0f);
        detection.source->SetMinDistance(1.0f);

        auto& bark = m_world->GetComponent<BarkSoundComponent>(m_enemyEntity);
        bark.source = std::make_unique<SpatialAudioSource>();
        bark.source->Initialize("Resources/Audio/enemy_bark.mp3", enemyPos);
        bark.source->SetVolume(0.375f);
        bark.source->SetMaxDistance(35.0f);
        bark.source->SetMinDistance(1.0f);

        // Set NavMesh
        auto& pathfinding = m_world->GetComponent<PathfindingComponent>(m_enemyEntity);
        navMeshManager = engine->GetNavMgr();
        if (navMeshManager && navMeshManager->GetNavMesh()) {
            pathfinding.navMesh = navMeshManager->GetNavMesh();
        }

        // Set jumpscare duration from animation data
        auto& jumpscareComp = m_world->GetComponent<EnemyJumpscareComponent>(m_enemyEntity);
        jumpscareComp.duration = jumpscareDuration;
    }

    // Orb entities (each needs its own model)
    std::vector<Vector3> orbPositions;
    if (JsonLoader::LoadOrbPositions("Resources/Models/orb/orb_positions.json", orbPositions)) {
        for (const auto& pos : orbPositions) {
            ECS::Entity orbEntity = EntityFactory::CreateOrbEntity(*m_world, pos, camera_);

            // Load orb model
            auto orbModel = engine->CreateAnim();
            orbModel->LoadFromFile("Resources/Models/orb", "orbtest.gltf");
            auto orbObj = engine->CreateObj3();
            orbObj->SetModel(static_cast<Model*>(orbModel.get()));
            orbObj->SetAnimatedModel(orbModel.get());
            orbObj->SetPosition(pos);
            orbObj->SetScale({1.0f, 1.0f, 1.0f});
            orbObj->SetEnableLighting(true);
            orbObj->SetCamera(camera_);
            orbObj->SetEmissiveFactor({20.0f, 20.0f, 20.0f});
            orbObj->Update();

            auto& orbAnim = m_world->GetComponent<AnimatedModelComponent>(orbEntity);
            orbAnim.animatedModel = std::move(orbModel);
            auto& orbRenderer = m_world->GetComponent<MeshRendererComponent>(orbEntity);
            orbRenderer.object3d = std::move(orbObj);

            m_orbEntities.push_back(orbEntity);
        }
    }

    // Scene object entities
    for (auto& obj : sceneObjects) {
        ECS::Entity sceneEntity = EntityFactory::CreateSceneObjectEntity(*m_world, std::move(obj));
        m_sceneObjectEntities.push_back(sceneEntity);
    }

    // Game state entity
    m_gameStateEntity = EntityFactory::CreateGameStateEntity(*m_world);
    auto& respawn = m_world->GetComponent<RespawnStateComponent>(m_gameStateEntity);
    respawn.playerInitialPosition = playerPos;
    if (!m_sceneData.enemies.empty()) {
        respawn.enemyInitialPosition = m_sceneData.enemies[0].position;
    }

    // Skybox component on game state entity
    if (skybox) {
        auto& skyboxComp = m_world->GetComponent<SkyboxComponent>(m_gameStateEntity);
        skyboxComp.skybox = std::move(skybox);
        skyboxComp.enabled = skyboxEnabled;
    }

    // Post-process chain on game state entity
    if (postProcess || horrorEffect) {
        auto& ppChain = m_world->GetComponent<PostProcessChainComponent>(m_gameStateEntity);
        ppChain.psxEffect = std::move(postProcess);
        ppChain.horrorEffect = std::move(horrorEffect);
        ppChain.fisheyeStrength = fisheyeStrength;
        ppChain.fisheyeRadius = fisheyeRadius;
    }

    // Minimap
    auto minimap = std::make_unique<Minimap>();
    minimap->Initialize(dxCommon_, srvManager_, mapPng, mapBounds, mapFrame);
    auto& minimapComp = m_world->GetComponent<MinimapComponent>(m_gameStateEntity);
    minimapComp.minimap = std::move(minimap);

    // Fade sprite
    auto fadeSprite = std::make_unique<Sprite>();
    fadeSprite->Initialize(spriteCommon_, "Resources/textures/white1x1.png");
    fadeSprite->SetPosition({0.0f, 0.0f});
    fadeSprite->SetSize({1280.0f, 720.0f});
    fadeSprite->setColor({0.0f, 0.0f, 0.0f, 0.0f});
    auto& fadeComp = m_world->GetComponent<FadeSpriteComponent>(m_gameStateEntity);
    fadeComp.sprite = std::move(fadeSprite);

    // Register LightManager as global resource (ownership transfer)
    m_world->SetResource<LightManager*>(lightManager.release());

    // Subtitle/Tutorial
    auto bitmapFont = std::make_unique<BitmapFont>();
    bitmapFont->Initialize(spriteCommon_, "Resources/font/Honoka-Shin-Maru-Gothic_R_16.fnt");
    auto subtitleManager = std::make_unique<SubtitleManager>();
    subtitleManager->Initialize(spriteCommon_, bitmapFont.get());
    subtitleManager->SetSteps({
        {L"ここがみんなが言っていた夢か...", 2.5f, 0.05f},
        {L"SNSで見た情報だと、捕まらないように\nすべてのオーブを集めればいいらしい。", 4.0f, 0.05f},
        {L"ただ厄介なのがあの青い熊だ", 2.0f, 0.05f},
        {L"どうやらあの熊は追いかけるスピードが速いようだ", 2.0f, 0.05f},
        {L"通路が結構入り組んでいるのを使って、なんとか対策できないだろうか。", 3.0f, 0.05f},
        {L"とりあえず、奴の足音に注意して集めよう", 2.0f, 0.10f},
    });
    subtitleManager->Start();

    auto& subtitleComp = m_world->GetComponent<SubtitleUIComponent>(m_gameStateEntity);
    subtitleComp.bitmapFont = std::move(bitmapFont);
    subtitleComp.subtitleManager = std::move(subtitleManager);

    // Minimap専用BitmapFontを生成（Subtitle側と共有するとBeginDrawでスプライトが上書きされる）
    {
        auto minimapFont = std::make_unique<BitmapFont>();
        minimapFont->Initialize(spriteCommon_, "Resources/font/Honoka-Shin-Maru-Gothic_R_16.fnt");
        if (minimapComp.minimap) {
            minimapComp.minimap->SetBitmapFont(minimapFont.get());
        }
        minimapComp.ownedBitmapFont = std::move(minimapFont);
    }

    auto& tutorial = m_world->GetComponent<TutorialComponent>(m_gameStateEntity);
    tutorial.isActive = true;

    // Settings menu
    {
        auto settingsMenu = std::make_unique<SettingsMenu>();
        settingsMenu->Initialize(spriteCommon_, engine->GetInput());
        // Set FPSCamera reference
        auto& fpsCam = m_world->GetComponent<FPSCameraComponent>(m_playerEntity);
        if (fpsCam.fpsCamera) {
            settingsMenu->SetFPSCamera(fpsCam.fpsCamera.get());
        }
        // BGMキー登録（ポーズ時にフェードアウト）
        if (!m_sceneData.audio.bgm.name.empty()) {
            settingsMenu->AddBGMKey(m_sceneData.audio.bgm.name);
        }
        settingsMenu->AddBGMKey("chaseBGM");
        auto& settingsComp = m_world->GetComponent<SettingsMenuComponent>(m_gameStateEntity);
        settingsComp.settingsMenu = settingsMenu.get();
        settingsComp.ownedSettingsMenu = std::move(settingsMenu);
    }

    // --- Continue復帰処理 ---
    if (s_gameProgress.isResuming) {
        // オーブ収集状態を復帰
        for (size_t i = 0; i < m_orbEntities.size() && i < s_gameProgress.collectedOrbs.size(); ++i) {
            if (s_gameProgress.collectedOrbs[i] && m_orbEntities[i].IsValid()) {
                auto& orb = m_world->GetComponent<OrbComponent>(m_orbEntities[i]);
                orb.isCollected = true;
            }
        }

        // ステルス状態を復帰
        m_world->ForEach<EnemyTag, StealthComponent>(
            [](Entity e, EnemyTag&, StealthComponent& stealth) {
                stealth.stealthEnabled = s_gameProgress.stealthEnabled;
                // 復帰猶予を開始
                stealth.isRevivalGrace = true;
                stealth.revivalGraceTimer = 0.0f;
            }
        );

        // ステルスチュートリアル状態を復帰
        auto& stealthTut = m_world->GetComponent<StealthTutorialComponent>(m_gameStateEntity);
        stealthTut.triggered = s_gameProgress.stealthTutorialTriggered;

        // チュートリアル字幕をスキップ（復帰時は不要）
        tutorial.isFinished = true;
        subtitleComp.subtitleManager->SetSteps({});
        subtitleComp.subtitleManager->Start(); // 空ステップで即完了

        // 敵AIは猶予中は非アクティブ
        m_world->ForEach<EnemyTag, EnemyAIComponent>(
            [](Entity e, EnemyTag&, EnemyAIComponent& ai) {
                ai.isActive = false;
            }
        );

        // 移動ロック不要（チュートリアルスキップ済み）
        auto& jumpscareVictim = m_world->GetComponent<JumpscareVictimComponent>(m_playerEntity);
        jumpscareVictim.isInJumpscare = false;

        s_gameProgress.isResuming = false;
    } else {
        // 通常開始：チュートリアル中は移動ロック
        auto& jumpscareVictim = m_world->GetComponent<JumpscareVictimComponent>(m_playerEntity);
        jumpscareVictim.isInJumpscare = true;
    }

    // Orb collection audio
    AudioManager::GetInstance()->LoadMP3("orbGet", "Resources/Audio/get.mp3");
    AudioManager::GetInstance()->SetVolume("orbGet", 0.125f);

    // Register all update systems
    RegisterSystems();

    // Create render systems (called separately in Draw)
    m_renderSystem = std::make_unique<ECS::RenderSystem>();
    m_uiRenderSystem = std::make_unique<ECS::UIRenderSystem>();
}

void GamePlayScene::RegisterSystems() {
    // Input (0-99)
    m_world->RegisterSystem(std::make_unique<ECS::InputSystem>(), 0);
    m_world->RegisterSystem(std::make_unique<ECS::DebugInputSystem>(), 10);

    // Physics (100-199)
    m_world->RegisterSystem(std::make_unique<ECS::PlayerMovementSystem>(), 100);
    m_world->RegisterSystem(std::make_unique<ECS::GravitySystem>(), 110);
    m_world->RegisterSystem(std::make_unique<ECS::GroundCollisionSystem>(), 120);
    m_world->RegisterSystem(std::make_unique<ECS::VelocityIntegrationSystem>(), 130);
    m_world->RegisterSystem(std::make_unique<ECS::RotationSmoothingSystem>(), 140);
    m_world->RegisterSystem(std::make_unique<ECS::CollisionResponseSystem>(), 150);

    // AI (200-299)
    m_world->RegisterSystem(std::make_unique<ECS::SoundDetectionSystem>(), 200);
    m_world->RegisterSystem(std::make_unique<ECS::VisionSystem>(), 210);
    m_world->RegisterSystem(std::make_unique<ECS::AIBehaviorSystem>(), 220);
    m_world->RegisterSystem(std::make_unique<ECS::PathfindingSystem>(), 230);
    m_world->RegisterSystem(std::make_unique<ECS::StuckDetectionSystem>(), 240);
    m_world->RegisterSystem(std::make_unique<ECS::StealthSystem>(), 250);
    m_world->RegisterSystem(std::make_unique<ECS::AmbushWarpSystem>(), 260);

    // Gameplay (300-399)
    m_world->RegisterSystem(std::make_unique<ECS::OrbCollectionSystem>(), 300);
    m_world->RegisterSystem(std::make_unique<ECS::FloatingAnimationSystem>(), 310);
    m_world->RegisterSystem(std::make_unique<ECS::SpotLightGlowSystem>(), 320);
    m_world->RegisterSystem(std::make_unique<ECS::JumpscareSystem>(), 330);
    m_world->RegisterSystem(std::make_unique<ECS::RespawnSystem>(), 340);
    m_world->RegisterSystem(std::make_unique<ECS::FearEffectSystem>(), 350);
    m_world->RegisterSystem(std::make_unique<ECS::TutorialSystem>(), 360);
    m_world->RegisterSystem(std::make_unique<ECS::StealthTutorialSystem>(), 365);

    // Animation (400)
    m_world->RegisterSystem(std::make_unique<ECS::AnimationSystem>(), 400);

    // Camera (500-510)
    m_world->RegisterSystem(std::make_unique<ECS::CameraSystem>(), 500);
    m_world->RegisterSystem(std::make_unique<ECS::CameraShakeSystem>(), 510);

    // Audio (600-650)
    m_world->RegisterSystem(std::make_unique<ECS::AudioListenerSystem>(), 600);
    m_world->RegisterSystem(std::make_unique<ECS::EnemyFootstepAudioSystem>(), 610);
    m_world->RegisterSystem(std::make_unique<ECS::DetectionSoundSystem>(), 620);
    m_world->RegisterSystem(std::make_unique<ECS::BarkSoundSystem>(), 630);
    m_world->RegisterSystem(std::make_unique<ECS::ChaseBGMSystem>(), 640);
    m_world->RegisterSystem(std::make_unique<ECS::PlayerFootstepSystem>(), 650);

    // Lighting (700-720)
    m_world->RegisterSystem(std::make_unique<ECS::LightingSystem>(), 700);
    m_world->RegisterSystem(std::make_unique<ECS::FlashlightSystem>(), 710);
    m_world->RegisterSystem(std::make_unique<ECS::LightDistributionSystem>(), 720);

    // Render preparation (800-810)
    m_world->RegisterSystem(std::make_unique<ECS::TransformSyncSystem>(), 800);
    m_world->RegisterSystem(std::make_unique<ECS::CullingSystemECS>(), 810);
}

void GamePlayScene::Update() {
    UnoEngine* engine = UnoEngine::GetInstance();
    const float deltaTime = engine->GetDelta();

    // ESC key: toggle settings menu
    if (engine->IsKeyTrig(DIK_ESCAPE) && m_gameStateEntity.IsValid()) {
        auto& gameState = m_world->GetComponent<GameStateComponent>(m_gameStateEntity);
        auto& settingsComp = m_world->GetComponent<SettingsMenuComponent>(m_gameStateEntity);
        if (settingsComp.settingsMenu) {
            if (settingsComp.settingsMenu->IsOpen()) {
                settingsComp.settingsMenu->Close();
                gameState.isPaused = false;
            } else {
                settingsComp.settingsMenu->Open();
                gameState.isPaused = true;
            }
        }
    }

    // When paused, only update settings menu
    if (m_gameStateEntity.IsValid()) {
        auto& gameState = m_world->GetComponent<GameStateComponent>(m_gameStateEntity);
        if (gameState.isPaused) {
            auto& settingsComp = m_world->GetComponent<SettingsMenuComponent>(m_gameStateEntity);
            if (settingsComp.settingsMenu) {
                settingsComp.settingsMenu->Update(deltaTime);
                // ×ボタンで閉じた場合、ポーズを解除
                if (!settingsComp.settingsMenu->IsOpen()) {
                    gameState.isPaused = false;
                }
            }
            return;
        }
    }

#ifdef _DEBUG
    // M key: toggle NavMesh debug window
    if (engine->IsKeyTrig(DIK_M)) {
        m_showNavMeshDebug = !m_showNavMeshDebug;
    }
    // F key: toggle LightManager debug window
    if (engine->IsKeyTrig(DIK_F)) {
        auto* lightManager = m_world->GetResource<LightManager*>();
        if (lightManager) {
            lightManager->ToggleDebugDisplay();
        }
    }
#endif

    // Game over fading: freeze all game logic, only update fade timer
    if (m_gameStateEntity.IsValid()) {
        auto& gameState = m_world->GetComponent<GameStateComponent>(m_gameStateEntity);
        if (gameState.gameOverFading) {
            // Stop BGM on first frame of game over
            if (gameState.gameOverFadeTimer == 0.0f) {
                UnoEngine* engine2 = UnoEngine::GetInstance();
                engine2->StopAudio("chaseBGM");
                if (!m_sceneData.audio.bgm.name.empty()) {
                    engine2->StopAudio(m_sceneData.audio.bgm.name);
                }
            }
            gameState.gameOverFadeTimer += deltaTime;
            constexpr float kGameOverFadeDuration = 2.0f;
            if (gameState.gameOverFadeTimer >= kGameOverFadeDuration) {
                // Continue用にゲーム進行を保存
                s_gameProgress.isResuming = true;
                s_gameProgress.collectedOrbs.clear();
                for (auto& orbEntity : m_orbEntities) {
                    if (orbEntity.IsValid() && m_world->HasComponent<OrbComponent>(orbEntity)) {
                        auto& orb = m_world->GetComponent<OrbComponent>(orbEntity);
                        s_gameProgress.collectedOrbs.push_back(orb.isCollected);
                    } else {
                        s_gameProgress.collectedOrbs.push_back(false);
                    }
                }
                m_world->ForEach<EnemyTag, StealthComponent>(
                    [](Entity e, EnemyTag&, StealthComponent& stealth) {
                        s_gameProgress.stealthEnabled = stealth.stealthEnabled;
                    }
                );
                m_world->ForEach<StealthTutorialComponent>(
                    [](Entity e, StealthTutorialComponent& st) {
                        s_gameProgress.stealthTutorialTriggered = st.triggered;
                    }
                );

                sceneManager_->ChangeScene("GameOver");
            }
            return; // Skip all game logic during game over fade
        }
    }

    // PostProcess resize on window size change
    {
        static uint32_t prevWidth = 0, prevHeight = 0;
        uint32_t curWidth = dxCommon_->GetCurrentWindowWidth();
        uint32_t curHeight = dxCommon_->GetCurrentWindowHeight();
        if (prevWidth != curWidth || prevHeight != curHeight) {
            if (prevWidth != 0) {
                m_world->ForEach<PostProcessChainComponent>(
                    [](ECS::Entity entity, PostProcessChainComponent& pp) {
                        if (pp.psxEffect) pp.psxEffect->ResizeRenderTarget();
                        if (pp.horrorEffect) pp.horrorEffect->ResizeRenderTarget();
                    }
                );
            }
            prevWidth = curWidth;
            prevHeight = curHeight;
        }
    }

    // All game logic via ECS
    m_world->UpdateSystems(deltaTime);

    // Ending fade-out: all orbs collected → fade to black → EndingScene
    if (m_gameStateEntity.IsValid()) {
        auto& gameState = m_world->GetComponent<GameStateComponent>(m_gameStateEntity);
        if (gameState.allOrbsCollected) {
            gameState.endingFadeTimer += deltaTime;
            constexpr float kEndingFadeDuration = 2.0f;
            float alpha = gameState.endingFadeTimer / kEndingFadeDuration;
            if (alpha >= 1.0f) {
                alpha = 1.0f;
                sceneManager_->ChangeScene("Ending");
            }
            // Write to respawn fadeAlpha so UIRenderSystem draws the fade sprite
            auto& respawn = m_world->GetComponent<RespawnStateComponent>(m_gameStateEntity);
            respawn.fadeAlpha = alpha;
        }
    }

    // NavMesh update
    engine->UpdateNavMesh();
}

void GamePlayScene::Draw() {
    // Render and UI via ECS render systems
    m_renderSystem->Update(*m_world, 0.0f);
    m_uiRenderSystem->Update(*m_world, 0.0f);

#ifdef _DEBUG
    // ImGui debug (NavMesh, Enemy AI, etc.)
    if (m_showNavMeshDebug) {
        ImGui::Begin("NavMesh Debug (M key)");
        UnoEngine* engine = UnoEngine::GetInstance();
        NavMeshManager* navMeshManager = engine->GetNavMgr();

        if (navMeshManager) {
            NavMesh* navMesh = navMeshManager->GetNavMesh();
            if (navMesh) {
                ImGui::Text("NavMesh Status: %s", navMesh->IsValid() ? "Valid" : "Invalid");
            }

            // NavMesh visualization toggle
            bool showViz = engine->IsNavVis();
            if (ImGui::Checkbox("Show NavMesh Visualization", &showViz)) {
                if (showViz) {
                    engine->CreateNavVis();
                    engine->SetNavVis(true);
                } else {
                    engine->SetNavVis(false);
                }
            }

            // Grid preview
            static bool showPreview = false;
            static bool showGrid = false;
            static bool showBoundingBox = false;
            if (ImGui::Checkbox("Show Grid Preview", &showPreview)) {
                navMeshManager->SetDebugPreviewEnabled(showPreview);
            }
            if (showPreview) {
                ImGui::Checkbox("Show Grid (Cyan)", &showGrid);
                ImGui::Checkbox("Show Bounding Box (Yellow)", &showBoundingBox);

                Entity enemyEntity = m_world->FindEntityWith<EnemyTag>();
                Vector3 center = {0.0f, 3.0f, 15.55f};
                if (enemyEntity.IsValid()) {
                    center = m_world->GetComponent<TransformComponent>(enemyEntity).position;
                }

                NavMeshBuildSettings& settings = engine->GetNavSet();
                float displayRadius = (std::max)(settings.agentRadius * 5.0f, 10.0f);
                Vector3 minBounds = {center.x - displayRadius, center.y - settings.agentHeight, center.z - displayRadius};
                Vector3 maxBounds = {center.x + displayRadius, center.y + settings.agentHeight, center.z + displayRadius};
                navMeshManager->SetPreviewBounds(minBounds, maxBounds);
                navMeshManager->CreateDebugPreview(engine->GetDXCom(), engine->GetCamera(), settings, center, showBoundingBox, showGrid);
            }
        }

        if (!m_navMeshLogs.empty()) {
            ImGui::BeginChild("Logs", ImVec2(0, 200), true);
            for (const auto& log : m_navMeshLogs) {
                ImGui::Text("%s", log.c_str());
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    auto* lightManager = m_world->GetResource<LightManager*>();
    if (lightManager) {
        lightManager->DrawImGui();
    }

    // Game State debug
    if (m_gameStateEntity.IsValid()) {
        auto& gameState = m_world->GetComponent<GameStateComponent>(m_gameStateEntity);
        ImGui::Begin("Game State Debug");
        ImGui::Checkbox("allOrbsCollected", &gameState.allOrbsCollected);
        ImGui::End();
    }

    // Ambush Warp debug
    m_world->ForEach<EnemyTag, TransformComponent, AmbushWarpComponent, EnemyAIComponent>(
        [this](Entity entity, EnemyTag&, TransformComponent& enemyTransform,
               AmbushWarpComponent& ambush, EnemyAIComponent& ai) {
            ImGui::Begin("Ambush Warp Debug");

            // Safety timer progress bar
            float safetyRatio = ambush.safetyTimer / AmbushWarpComponent::kSafetyThreshold;
            ImGui::ProgressBar(safetyRatio, ImVec2(-1, 0),
                (std::to_string((int)ambush.safetyTimer) + "s / " +
                 std::to_string((int)AmbushWarpComponent::kSafetyThreshold) + "s").c_str());
            ImGui::Text("Safety Timer: %.1f / %.1f", ambush.safetyTimer, AmbushWarpComponent::kSafetyThreshold);
            ImGui::Text("Cooldown: %.1f / %.1f", ambush.warpCooldownTimer, AmbushWarpComponent::kWarpCooldown);

            // Enemy-player distance
            Entity playerEntity = m_world->FindEntityWith<PlayerTag>();
            if (playerEntity.IsValid()) {
                auto& playerTransform = m_world->GetComponent<TransformComponent>(playerEntity);
                float dx = enemyTransform.position.x - playerTransform.position.x;
                float dy = enemyTransform.position.y - playerTransform.position.y;
                float dz = enemyTransform.position.z - playerTransform.position.z;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                bool distOk = dist >= AmbushWarpComponent::kMinEnemyDistance;
                ImGui::Text("Enemy Distance: %.1fm (need >= %.0fm) %s",
                    dist, AmbushWarpComponent::kMinEnemyDistance, distOk ? "OK" : "TOO CLOSE");
            }

            // Condition flags
            ImGui::Separator();
            ImGui::Text("AI State: %s",
                ai.isChasing ? "CHASING" : ai.isSearching ? "SEARCHING" : "PATROL/IDLE");
            ImGui::Text("AI Active: %s", ai.isActive ? "Yes" : "No");

            bool timerReady = ambush.safetyTimer >= AmbushWarpComponent::kSafetyThreshold;
            bool cooldownReady = ambush.warpCooldownTimer <= 0.0f;
            ImGui::TextColored(timerReady ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                "Timer Ready: %s", timerReady ? "YES" : "NO");
            ImGui::TextColored(cooldownReady ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                "Cooldown Ready: %s", cooldownReady ? "YES" : "NO");
            ImGui::TextColored(!ai.isChasing ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                "Not Chasing: %s", !ai.isChasing ? "YES" : "NO");

            ImGui::End();
        }
    );
#endif
}

void GamePlayScene::Finalize() {
    UnoEngine* engine = UnoEngine::GetInstance();

    // Stop BGM
    if (!m_sceneData.audio.bgm.name.empty()) {
        engine->StopAudio(m_sceneData.audio.bgm.name);
    }
    // Stop chase BGM (may still be playing if orbs collected during chase)
    engine->StopAudio("chaseBGM");

    // Clear collision manager before destroying entities (prevents dangling pointers)
    if (auto* colMgr = Collision::AABBCollisionManager::GetInstance()) {
        colMgr->Clear();
    }

    // Destroy all entities
    for (auto& entity : m_orbEntities) {
        m_world->DestroyEntity(entity);
    }
    for (auto& entity : m_sceneObjectEntities) {
        m_world->DestroyEntity(entity);
    }
    if (m_playerEntity.IsValid()) m_world->DestroyEntity(m_playerEntity);
    if (m_enemyEntity.IsValid()) m_world->DestroyEntity(m_enemyEntity);
    if (m_gameStateEntity.IsValid()) m_world->DestroyEntity(m_gameStateEntity);

    m_orbEntities.clear();
    m_sceneObjectEntities.clear();

    // Clear systems
    m_world->ClearSystems();
    m_renderSystem.reset();
    m_uiRenderSystem.reset();
}

void GamePlayScene::AddNavMeshLog(const std::string& message) {
    m_navMeshLogs.push_back(message);
    if (m_navMeshLogs.size() > 1000) {
        m_navMeshLogs.erase(m_navMeshLogs.begin());
    }
}

void GamePlayScene::ClearNavMeshLogs() {
    m_navMeshLogs.clear();
}
