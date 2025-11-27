#pragma once

#include "../../Engine/Core/GameObject.h"
#include "../../Engine/Core/Camera.h"
#include "../../Engine/Math/Math.h"
#include <imgui.h>
#include "ImGuizmo.h"

namespace UnoEngine {

// ギズモ操作モード
enum class GizmoOperation {
    Translate,
    Rotate,
    Scale
};

// ギズモ空間モード
enum class GizmoMode {
    Local,
    World
};

// ギズモシステム - オブジェクトの変換操作を管理
class GizmoSystem {
public:
    GizmoSystem() = default;
    ~GizmoSystem() = default;

    // 初期化（ImGuiコンテキスト設定後に呼ぶ）
    void Initialize();

    // ギズモを描画・操作
    // 戻り値: オブジェクトが変更された場合true
    bool RenderGizmo(GameObject* selectedObject, Camera* camera,
                     float viewportX, float viewportY,
                     float viewportWidth, float viewportHeight);

    // 操作モード設定
    void SetOperation(GizmoOperation op) { operation_ = op; }
    GizmoOperation GetOperation() const { return operation_; }

    // 空間モード設定
    void SetMode(GizmoMode mode) { mode_ = mode; }
    GizmoMode GetMode() const { return mode_; }

    // スナップ設定
    void SetSnapEnabled(bool enabled) { snapEnabled_ = enabled; }
    bool IsSnapEnabled() const { return snapEnabled_; }

    void SetTranslationSnap(float snap) { translationSnap_ = snap; }
    void SetRotationSnap(float snap) { rotationSnap_ = snap; }
    void SetScaleSnap(float snap) { scaleSnap_ = snap; }

    // ギズモがアクティブかどうか
    bool IsUsing() const;
    bool IsOver() const;

private:
    // ImGuizmoのOPERATIONに変換
    ImGuizmo::OPERATION ToImGuizmoOperation() const;
    ImGuizmo::MODE ToImGuizmoMode() const;

private:
    GizmoOperation operation_ = GizmoOperation::Translate;
    GizmoMode mode_ = GizmoMode::World;

    bool snapEnabled_ = false;
    float translationSnap_ = 1.0f;
    float rotationSnap_ = 15.0f;
    float scaleSnap_ = 0.1f;
};

} // namespace UnoEngine
