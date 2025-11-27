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

    // Play/Pause control
    void SetPlaying(bool playing) { isPlaying_ = playing; }
    bool IsPlaying() const { return isPlaying_; }

private:
    bool isPlaying_ = true;  // 初期は再生状態（0.1秒後にオフ）
    float elapsedTime_ = 0.0f;
    bool autoStopTriggered_ = false;
    static constexpr float AUTO_STOP_TIME = 0.1f;
};

} // namespace UnoEngine
