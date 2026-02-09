#pragma once
#include "ECS/World.h"
#include "ECS/Entity.h"
#include "Mymath.h"
#include <string>
#include <vector>

class Camera;
class Object3d;
struct EnemyAIConfig;

namespace EntityFactory {

// Player entity: Transform + Movement + Physics + Camera + Animation + Audio + Collision
ECS::Entity CreatePlayerEntity(ECS::World& world, const Vector3& position, Camera* camera);

// Enemy entity: Transform + AI + Vision + Sound + Pathfinding + Audio + Animation + Collision
ECS::Entity CreateEnemyEntity(ECS::World& world, const Vector3& position,
                               const EnemyAIConfig& aiConfig, Camera* camera);

// Orb entity: Transform + FloatingAnim + Collectible + Render
ECS::Entity CreateOrbEntity(ECS::World& world, const Vector3& position, Camera* camera);

// Static scene object: Transform + Render
ECS::Entity CreateSceneObjectEntity(ECS::World& world, std::unique_ptr<Object3d> object3d);

// Game state singleton entity: GameState + Respawn + Fear + Tutorial + PostProcess + UI
ECS::Entity CreateGameStateEntity(ECS::World& world);

} // namespace EntityFactory
