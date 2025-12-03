#include "GizmoSystem.h"
#include <imgui.h>
#include <cmath>
#include <cstring>

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

    // 軸の感度を上げる
    ImGuizmo::AllowAxisFlip(true);

    // ギズモのサイズを大きく設定
    ImGuizmo::SetGizmoSizeClipSpace(0.25f);

    // カメラ行列を取得
    float viewMatrix[16];
    float projMatrix[16];
    const_cast<Camera*>(camera)->GetViewMatrix().ToFloatArray(viewMatrix);
    camera->GetProjectionMatrix().ToFloatArray(projMatrix);

    // オブジェクトのTransformを取得
    auto& transform = selectedObject->GetTransform();

    // 操作前のローカル値を保存
    Vector3 oldLocalPos = transform.GetLocalPosition();
    Quaternion oldLocalRot = transform.GetLocalRotation();
    Vector3 oldLocalScale = transform.GetLocalScale();

    // ギズモ用の行列を構築（ワールド行列を使用）
    float gizmoMatrix[16];
    transform.GetWorldMatrix().ToFloatArray(gizmoMatrix);

    // スナップ値
    float* snapPtr = nullptr;
    float snapValues[3] = { 0, 0, 0 };

    if (snapEnabled_ || io.KeyCtrl) {
        switch (operation_) {
            case GizmoOperation::Translate:
                snapValues[0] = snapValues[1] = snapValues[2] = 1.0f;
                break;
            case GizmoOperation::Rotate:
                snapValues[0] = snapValues[1] = snapValues[2] = 15.0f;
                break;
            case GizmoOperation::Scale:
                snapValues[0] = snapValues[1] = snapValues[2] = 1.0f;
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
        gizmoMatrix,
        deltaMatrix,
        snapPtr
    );

    // 操作があった場合、Transformを更新
    if (manipulated) {
        // ImGuizmoの結果を分解
        float newTranslation[3], newRotation[3], newScale[3];
        ImGuizmo::DecomposeMatrixToComponents(gizmoMatrix, newTranslation, newRotation, newScale);

        // NaNチェック
        bool hasNaN = std::isnan(newTranslation[0]) || std::isnan(newTranslation[1]) || std::isnan(newTranslation[2]) ||
                      std::isnan(newRotation[0]) || std::isnan(newRotation[1]) || std::isnan(newRotation[2]) ||
                      std::isnan(newScale[0]) || std::isnan(newScale[1]) || std::isnan(newScale[2]);

        if (hasNaN) {
            return false;
        }

        switch (operation_) {
            case GizmoOperation::Translate: {
                // 新しいワールド位置を取得
                Vector3 newWorldPos(newTranslation[0], newTranslation[1], newTranslation[2]);

                if (transform.GetParent() == nullptr) {
                    transform.SetLocalPosition(newWorldPos);
                } else {
                    transform.SetPosition(newWorldPos);
                }
                break;
            }
            case GizmoOperation::Rotate: {
                // オイラー角から新しい回転を作成
                constexpr float DEG_TO_RAD = 0.0174532925f;
                float radX = newRotation[0] * DEG_TO_RAD;
                float radY = newRotation[1] * DEG_TO_RAD;
                float radZ = newRotation[2] * DEG_TO_RAD;

                Quaternion newRot = Quaternion::RotationRollPitchYaw(radX, radY, radZ);
                newRot = newRot.Normalize();

                if (!std::isnan(newRot.GetX()) && !std::isnan(newRot.GetY()) &&
                    !std::isnan(newRot.GetZ()) && !std::isnan(newRot.GetW())) {
                    // 位置を先に設定してから回転を設定（位置が変わらないように）
                    transform.SetLocalPosition(oldLocalPos);
                    transform.SetLocalRotation(newRot);
                }
                break;
            }
            case GizmoOperation::Scale: {
                Vector3 newScaleVec(newScale[0], newScale[1], newScale[2]);

                // 異常なスケールをチェック
                if (newScaleVec.GetX() < 0.001f || newScaleVec.GetY() < 0.001f || newScaleVec.GetZ() < 0.001f ||
                    newScaleVec.GetX() > 1000.0f || newScaleVec.GetY() > 1000.0f || newScaleVec.GetZ() > 1000.0f) {
                    break;
                }

                transform.SetLocalScale(newScaleVec);
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
