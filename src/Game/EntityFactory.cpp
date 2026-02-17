#include "EntityFactory.h"
#include "UnoEngine.h"
#include "GameObject/EnemyAIConfig.h"
#include "GameObject/FPSCamera.h"
#include "UI/BitmapFont.h"
#include "UI/SubtitleManager.h"
#include "UI/Minimap.h"
#include "UI/SettingsMenu.h"

// ECS Components
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

namespace EntityFactory {

ECS::Entity CreatePlayerEntity(ECS::World& world, const Vector3& position, Camera* camera) {
    auto entity = world.CreateEntity();

    world.AddComponent(entity, ECS::PlayerTag{});

    world.AddComponent(entity, ECS::TransformComponent{
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    world.AddComponent(entity, ECS::PreviousPositionComponent{.previousPosition = position});
    world.AddComponent(entity, ECS::VelocityComponent{});
    world.AddComponent(entity, ECS::GravityComponent{.gravity = -25.0f, .isGrounded = false});
    world.AddComponent(entity, ECS::RotationSmoothingComponent{});

    world.AddComponent(entity, ECS::PlayerMovementComponent{});
    world.AddComponent(entity, ECS::PlayerInputComponent{});
    world.AddComponent(entity, ECS::JumpComponent{.jumpPower = 10.0f});
    world.AddComponent(entity, ECS::PlayerFootstepComponent{});
    world.AddComponent(entity, ECS::JumpscareVictimComponent{});

    // Animation (model loaded separately)
    world.AddComponent(entity, ECS::AnimatedModelComponent{});

    // Mesh renderer (Object3d created separately)
    world.AddComponent(entity, ECS::MeshRendererComponent{});

    // Camera
    auto fpsCamera = std::make_unique<FPSCamera>();
    fpsCamera->Initialize(true);
    world.AddComponent(entity, ECS::FPSCameraComponent{
        .fpsCamera = std::move(fpsCamera),
        .isFPSMode = true
    });
    world.AddComponent(entity, ECS::CameraFollowComponent{});
    world.AddComponent(entity, ECS::CameraShakeComponent{});

    // Collision
    world.AddComponent(entity, ECS::AABBColliderComponent{.enabled = true, .name = "Player"});

    // Audio listener
    world.AddComponent(entity, ECS::AudioListenerComponent{});

    return entity;
}

ECS::Entity CreateEnemyEntity(ECS::World& world, const Vector3& position,
                               const EnemyAIConfig& aiConfig, Camera* camera) {
    auto entity = world.CreateEntity();

    world.AddComponent(entity, ECS::EnemyTag{});

    world.AddComponent(entity, ECS::TransformComponent{
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    world.AddComponent(entity, ECS::PreviousPositionComponent{.previousPosition = position});
    // Note: Enemy does NOT use VelocityComponent/GravityComponent
    // PathfindingSystem directly controls position via NavMeshHelper::FollowPath
    world.AddComponent(entity, ECS::RotationSmoothingComponent{.externalControl = true});

    // AI
    world.AddComponent(entity, ECS::EnemyAIComponent{
        .intelligence = aiConfig.intelligence,
        .aggressiveness = aiConfig.aggressiveness,
        .mobility = aiConfig.mobility,
        .patrolMobility = aiConfig.patrolMobility,
        .searchMobility = aiConfig.searchMobility,
        .moveSpeed = aiConfig.mobility,
        .patrolMoveSpeed = aiConfig.patrolMobility,
        .searchMoveSpeed = aiConfig.searchMobility
    });

    world.AddComponent(entity, ECS::VisionComponent{});
    world.AddComponent(entity, ECS::SoundDetectionComponent{});
    world.AddComponent(entity, ECS::PathfindingComponent{});
    world.AddComponent(entity, ECS::StuckDetectionComponent{});
    world.AddComponent(entity, ECS::EnemyJumpscareComponent{});
    world.AddComponent(entity, ECS::StealthComponent{});
    world.AddComponent(entity, ECS::AmbushWarpComponent{});

    // Animation & Rendering
    world.AddComponent(entity, ECS::AnimatedModelComponent{});
    world.AddComponent(entity, ECS::MeshRendererComponent{});

    // Audio
    world.AddComponent(entity, ECS::EnemyFootstepAudioComponent{});
    world.AddComponent(entity, ECS::DetectionSoundComponent{});
    world.AddComponent(entity, ECS::BarkSoundComponent{});
    world.AddComponent(entity, ECS::ChaseBGMComponent{});

    // Collision
    world.AddComponent(entity, ECS::AABBColliderComponent{.enabled = true, .name = "Enemy"});

#ifdef _DEBUG
    world.AddComponent(entity, ECS::EnemyDebugComponent{});
#endif

    return entity;
}

ECS::Entity CreateOrbEntity(ECS::World& world, const Vector3& position, Camera* camera) {
    auto entity = world.CreateEntity();

    world.AddComponent(entity, ECS::CollectibleTag{});

    world.AddComponent(entity, ECS::TransformComponent{
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    world.AddComponent(entity, ECS::OrbComponent{});
    world.AddComponent(entity, ECS::FloatingAnimationComponent{
        .basePosition = position
    });
    world.AddComponent(entity, ECS::RotatingComponent{.rotationSpeed = 2.0f});
    world.AddComponent(entity, ECS::SpotLightGlowComponent{});
    world.AddComponent(entity, ECS::MeshRendererComponent{});
    world.AddComponent(entity, ECS::AnimatedModelComponent{});

    return entity;
}

ECS::Entity CreateSceneObjectEntity(ECS::World& world, std::unique_ptr<Object3d> object3d) {
    auto entity = world.CreateEntity();

    world.AddComponent(entity, ECS::SceneObjectTag{});

    Vector3 pos = object3d->GetPosition();
    Vector3 rot = object3d->GetRotation();
    Vector3 scl = object3d->GetScale();

    world.AddComponent(entity, ECS::TransformComponent{
        .position = pos,
        .rotation = rot,
        .scale = scl
    });

    world.AddComponent(entity, ECS::MeshRendererComponent{
        .object3d = std::move(object3d),
        .visible = true
    });

    return entity;
}

ECS::Entity CreateGameStateEntity(ECS::World& world) {
    auto entity = world.CreateEntity();

    world.AddComponent(entity, ECS::GameStateComponent{});
    world.AddComponent(entity, ECS::RespawnStateComponent{});
    world.AddComponent(entity, ECS::FearEffectComponent{});
    world.AddComponent(entity, ECS::TutorialComponent{});
    world.AddComponent(entity, ECS::SkyboxComponent{});
    world.AddComponent(entity, ECS::PostProcessChainComponent{});
    world.AddComponent(entity, ECS::MinimapComponent{});
    world.AddComponent(entity, ECS::FadeSpriteComponent{});
    world.AddComponent(entity, ECS::SubtitleUIComponent{});
    world.AddComponent(entity, ECS::StealthTutorialComponent{});
    world.AddComponent(entity, ECS::SettingsMenuComponent{});

    return entity;
}

} // namespace EntityFactory
