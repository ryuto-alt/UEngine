#include "AIBehaviorSystem.h"
#include "ECS/World.h"
#include "UnoEngine.h"
#include "ECS/Components/TransformComponents.h"
#include "ECS/Components/EnemyComponents.h"
#include "ECS/Components/RenderComponents.h"

namespace ECS {

namespace {

void ChangeEnemyAnimation(AnimatedModelComponent& anim, const std::string& name) {
    if (!anim.animatedModel) return;
    anim.animatedModel->TransitionToAnimation(name, anim.blendDuration);
    anim.isBlending = true;
    anim.blendTimer = 0.0f;
    anim.currentAnimationName = name;

    // Run animation plays at 1.3x speed
    if (name == "Run") {
        anim.animationSpeed = 1.3f;
    } else if (name == "Walk") {
        anim.animationSpeed = 1.0f;
    }
}

} // anonymous namespace

void AIBehaviorSystem::Update(World& world, float deltaTime) {
    world.ForEach<EnemyTag, EnemyAIComponent, AnimatedModelComponent, EnemyJumpscareComponent>(
        [deltaTime](Entity entity, EnemyTag&, EnemyAIComponent& ai,
                   AnimatedModelComponent& anim, EnemyJumpscareComponent& jumpscare) {

            // Skip AI if inactive or during jumpscare
            if (!ai.isActive) return;
            if (jumpscare.isJumpscaring) return;

            // Track chase state transitions for animation changes
            bool justStartedChasing = ai.isChasing && !ai.wasChasing;
            bool justStoppedChasing = !ai.isChasing && ai.wasChasing;

            if (justStartedChasing) {
                ChangeEnemyAnimation(anim, "Run");
            }

            if (justStoppedChasing) {
                ChangeEnemyAnimation(anim, "Walk");
            }

            // Search mode starts with Walk animation
            if (ai.isSearching && !ai.isChasing && anim.currentAnimationName != "Walk") {
                ChangeEnemyAnimation(anim, "Walk");
            }

            ai.wasChasing = ai.isChasing;
        }
    );
}

} // namespace ECS
