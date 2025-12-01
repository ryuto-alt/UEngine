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

    // リスナー位置の取得（エディタオーバーライド対応）
    Vector3 GetListenerPosition() const;
    
    // リスナーの前方向を取得
    Vector3 GetListenerForward() const;
    
    // リスナーの上方向を取得
    Vector3 GetListenerUp() const;

    // エディタプレビュー用：リスナー位置・向きのオーバーライド
    void SetEditorOverridePosition(const Vector3& pos) {
        editorOverridePosition_ = pos;
        useEditorOverride_ = true;
    }
    void SetEditorOverrideOrientation(const Vector3& forward, const Vector3& up) {
        editorOverrideForward_ = forward;
        editorOverrideUp_ = up;
    }
    void ClearEditorOverride() { useEditorOverride_ = false; }
    bool IsUsingEditorOverride() const { return useEditorOverride_; }
    const Vector3& GetEditorOverridePosition() const { return editorOverridePosition_; }

private:
    static inline AudioListener* instance_ = nullptr;
    Vector3 editorOverridePosition_;
    Vector3 editorOverrideForward_ = Vector3(0.0f, 0.0f, 1.0f);
    Vector3 editorOverrideUp_ = Vector3(0.0f, 1.0f, 0.0f);
    bool useEditorOverride_ = false;
};

} // namespace UnoEngine
