#pragma once
#include "ECS/System.h"

namespace ECS {

// Enemy footstep audio via bone tracking (L/R foot landing detection)
class EnemyFootstepAudioSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "EnemyFootstepAudioSystem"; }
};

} // namespace ECS
