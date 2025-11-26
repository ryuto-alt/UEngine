#pragma once

#include "../Systems/ISystem.h"

namespace UnoEngine {

class AnimationSystem : public ISystem {
public:
    AnimationSystem() = default;
    ~AnimationSystem() override = default;

    void OnUpdate(Scene* scene, float deltaTime) override;
    
    // Animation system should run early to update bone matrices before rendering
    int GetPriority() const override { return 10; }
};

} // namespace UnoEngine
