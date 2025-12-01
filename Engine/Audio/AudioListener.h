#pragma once

#include "../Core/Component.h"

namespace UnoEngine {

// 3Dオーディオのリスナー位置を表すコンポーネント
// シーンに1つだけ存在すべき
class AudioListener : public Component {
public:
    AudioListener() { instance_ = this; }
    ~AudioListener() override { if (instance_ == this) instance_ = nullptr; }

    // シングルトンアクセス
    static AudioListener* GetInstance() { return instance_; }

    // Component lifecycle
    void Awake() override {}
    void Start() override {}
    void OnUpdate(float deltaTime) override {}
    void OnDestroy() override { if (instance_ == this) instance_ = nullptr; }

private:
    static inline AudioListener* instance_ = nullptr;
};

} // namespace UnoEngine
