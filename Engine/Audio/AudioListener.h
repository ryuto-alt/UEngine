#pragma once

#include "../Core/Component.h"
#include "../Math/Vector.h"

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

    // エディタプレビュー用：リスナー位置のオーバーライド
    void SetEditorOverridePosition(const Vector3& pos) {
        editorOverridePosition_ = pos;
        useEditorOverride_ = true;
    }
    void ClearEditorOverride() { useEditorOverride_ = false; }
    bool IsUsingEditorOverride() const { return useEditorOverride_; }
    const Vector3& GetEditorOverridePosition() const { return editorOverridePosition_; }

private:
    static inline AudioListener* instance_ = nullptr;
    Vector3 editorOverridePosition_;
    bool useEditorOverride_ = false;
};

} // namespace UnoEngine
