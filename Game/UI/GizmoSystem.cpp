#include "GizmoSystem.h"
#include <imgui.h>
#include <cmath>

namespace UnoEngine {

void GizmoSystem::Initialize() {
    // ImGuizmoコンテキストをImGuiと同期
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
}

bool GizmoSystem::RenderGizmo(GameObject* selectedObject, Camera* camera,
                              float viewportX, float viewportY,
                              float viewportWidth, float viewportHeight) {
    if (!selectedObject || !camera) {
        return false;
    }

    // 右クリック中はギズモを無効化（カメラ操作を優先）
    ImGuiIO& io = ImGui::GetIO();
    if (io.MouseDown[1]) {
        return false;
    }

    // 毎フレーム必ず呼び出す（前フレームの状態をクリア）
    ImGuizmo::BeginFrame();

    // ギズモの描画領域を設定
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportX, viewportY, viewportWidth, viewportHeight);
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::Enable(true);

    // 軸の感度を上げる（特にカメラ正面に近い軸の反応を良くする）
    ImGuizmo::AllowAxisFlip(true);

    // ギズモのサイズを大きく設定
    ImGuizmo::SetGizmoSizeClipSpace(0.25f);

    // カメラ行列を取得
    float viewMatrix[16];
    float projMatrix[16];
    const_cast<Camera*>(camera)->GetViewMatrix().ToFloatArray(viewMatrix);
    camera->GetProjectionMatrix().ToFloatArray(projMatrix);

    // オブジェクトのワールド行列を取得（ギズモ描画用）
    auto& transform = selectedObject->GetTransform();
    float objectMatrix[16];
    transform.GetWorldMatrix().ToFloatArray(objectMatrix);

    // 操作前のワールド座標を保存
    float oldTranslation[3], oldRotation[3], oldScale[3];
    ImGuizmo::DecomposeMatrixToComponents(objectMatrix, oldTranslation, oldRotation, oldScale);

    // スナップ値（CTRL押下時またはsnapEnabled時に有効）
    float* snapPtr = nullptr;
    float snapValues[3] = { 0, 0, 0 };

    if (snapEnabled_ || io.KeyCtrl) {
        switch (operation_) {
            case GizmoOperation::Translate:
                snapValues[0] = snapValues[1] = snapValues[2] = 1.0f;  // 1単位でスナップ
                break;
            case GizmoOperation::Rotate:
                snapValues[0] = snapValues[1] = snapValues[2] = 15.0f;  // 15度でスナップ
                break;
            case GizmoOperation::Scale:
                snapValues[0] = snapValues[1] = snapValues[2] = 1.0f;  // 1.0倍でスナップ
                break;
        }
        snapPtr = snapValues;
    }

    // ギズモを描画・操作
    float deltaMatrix[16];
    bool manipulated = ImGuizmo::Manipulate(
        viewMatrix,
        projMatrix,
        ToImGuizmoOperation(),
        ToImGuizmoMode(),
        objectMatrix,
        deltaMatrix,
        snapPtr
    );

    // 操作があった場合、Transformを更新
    if (manipulated) {
        float newTranslation[3], newRotation[3], newScale[3];
        ImGuizmo::DecomposeMatrixToComponents(objectMatrix, newTranslation, newRotation, newScale);

        switch (operation_) {
            case GizmoOperation::Translate: {
                // ワールド座標の差分をローカルに適用
                Vector3 delta(
                    newTranslation[0] - oldTranslation[0],
                    newTranslation[1] - oldTranslation[1],
                    newTranslation[2] - oldTranslation[2]
                );
                Vector3 localPos = transform.GetLocalPosition();
                transform.SetLocalPosition(localPos + delta);
                break;
            }
            case GizmoOperation::Rotate: {
                // 回転は差分ではなく直接設定（親がない場合はワールド=ローカル）
                constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;
                Quaternion quat = Quaternion::RotationRollPitchYaw(
                    newRotation[0] * DEG_TO_RAD,
                    newRotation[1] * DEG_TO_RAD,
                    newRotation[2] * DEG_TO_RAD
                );
                transform.SetLocalRotation(quat);
                break;
            }
            case GizmoOperation::Scale: {
                // スケールの差分をローカルに適用
                Vector3 localScale = transform.GetLocalScale();
                float scaleRatioX = (std::abs(oldScale[0]) > 0.0001f) ? newScale[0] / oldScale[0] : 1.0f;
                float scaleRatioY = (std::abs(oldScale[1]) > 0.0001f) ? newScale[1] / oldScale[1] : 1.0f;
                float scaleRatioZ = (std::abs(oldScale[2]) > 0.0001f) ? newScale[2] / oldScale[2] : 1.0f;
                transform.SetLocalScale(Vector3(
                    localScale.GetX() * scaleRatioX,
                    localScale.GetY() * scaleRatioY,
                    localScale.GetZ() * scaleRatioZ
                ));
                break;
            }
        }
    }

    return manipulated;
}

bool GizmoSystem::IsUsing() const {
    return ImGuizmo::IsUsing();
}

bool GizmoSystem::IsOver() const {
    return ImGuizmo::IsOver();
}

ImGuizmo::OPERATION GizmoSystem::ToImGuizmoOperation() const {
    switch (operation_) {
        case GizmoOperation::Translate: return ImGuizmo::TRANSLATE;
        case GizmoOperation::Rotate:    return ImGuizmo::ROTATE;
        case GizmoOperation::Scale:     return ImGuizmo::SCALE;
        default: return ImGuizmo::TRANSLATE;
    }
}

ImGuizmo::MODE GizmoSystem::ToImGuizmoMode() const {
    switch (mode_) {
        case GizmoMode::Local: return ImGuizmo::LOCAL;
        case GizmoMode::World: return ImGuizmo::WORLD;
        default: return ImGuizmo::WORLD;
    }
}

} // namespace UnoEngine
