#include "GizmoSystem.h"
#include <imgui.h>

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

    // オブジェクトのワールド行列を取得
    float objectMatrix[16];
    auto& transform = selectedObject->GetTransform();
    transform.GetWorldMatrix().ToFloatArray(objectMatrix);

    // スナップ値
    float* snapPtr = nullptr;
    float snapValues[3] = { 0, 0, 0 };

    if (snapEnabled_) {
        switch (operation_) {
            case GizmoOperation::Translate:
                snapValues[0] = snapValues[1] = snapValues[2] = translationSnap_;
                break;
            case GizmoOperation::Rotate:
                snapValues[0] = snapValues[1] = snapValues[2] = rotationSnap_;
                break;
            case GizmoOperation::Scale:
                snapValues[0] = snapValues[1] = snapValues[2] = scaleSnap_;
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

    // 操作があった場合、Transformを更新（操作タイプに応じて必要な値のみ更新）
    if (manipulated) {
        float translation[3], rotation[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(objectMatrix, translation, rotation, scale);

        switch (operation_) {
            case GizmoOperation::Translate:
                transform.SetLocalPosition(Vector3(translation[0], translation[1], translation[2]));
                break;
            case GizmoOperation::Rotate: {
                // 回転のみ更新（度からラジアンに変換）
                constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;
                Quaternion quat = Quaternion::RotationRollPitchYaw(
                    rotation[0] * DEG_TO_RAD,
                    rotation[1] * DEG_TO_RAD,
                    rotation[2] * DEG_TO_RAD
                );
                transform.SetLocalRotation(quat);
                break;
            }
            case GizmoOperation::Scale:
                transform.SetLocalScale(Vector3(scale[0], scale[1], scale[2]));
                break;
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
