#pragma once

#include "../../Engine/Core/Camera.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

namespace UnoEngine {

// エディタ用カメラコントローラー
// フリーカメラ、オービットカメラなどの操作を提供
class EditorCamera {
public:
    EditorCamera() = default;
    ~EditorCamera() = default;

    // カメラを設定
    void SetCamera(Camera* camera) { camera_ = camera; }
    Camera* GetCamera() const { return camera_; }

    // 毎フレーム更新（入力処理）
    void Update(float deltaTime);

    // 設定
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetRotateSpeed(float speed) { rotateSpeed_ = speed; }
    void SetScrollSpeed(float speed) { scrollSpeed_ = speed; }

    float GetMoveSpeed() const { return moveSpeed_; }
    float GetRotateSpeed() const { return rotateSpeed_; }
    float GetScrollSpeed() const { return scrollSpeed_; }

    // Viewport内でのみカメラ操作を有効にする
    void SetViewportHovered(bool hovered) { viewportHovered_ = hovered; }
    void SetViewportFocused(bool focused) { viewportFocused_ = focused; }
    
    // WASD移動を有効/無効にする（オブジェクト選択中は無効にする）
    void SetMovementEnabled(bool enabled) { movementEnabled_ = enabled; }

    // ビューポートの矩形を設定（マウスクリップ用）
    void SetViewportRect(float x, float y, float w, float h) {
        viewportRect_ = { x, y, w, h };
    }

    // カメラ操作中かどうか
    bool IsControlling() const { return isControlling_; }

    // オブジェクトにフォーカス（オービットの中心を設定）
    // resetAngle: trueで斜め上からの角度にリセット、falseで現在の角度を維持
    void FocusOn(const Vector3& targetPosition, float distance = 5.0f, bool resetAngle = false);

    // オービットターゲット設定
    void SetOrbitTarget(const Vector3& target) { orbitTarget_ = target; hasOrbitTarget_ = true; }

    // オービットターゲットをクリア（フリーカメラに戻る）
    void ClearOrbitTarget() { hasOrbitTarget_ = false; }

    // オービットターゲットがあるか
    bool HasOrbitTarget() const { return hasOrbitTarget_; }

    // 設定の保存/読み込み
    void SaveSettings(const std::string& filepath = "editor_camera_settings.json");
    void LoadSettings(const std::string& filepath = "editor_camera_settings.json");

private:
    // フリーカメラ操作（右クリック + WASD）
    void HandleFreeCameraMovement(float deltaTime);

    // マウス回転
    void HandleMouseRotation(float deltaTime);

    // スクロールズーム
    void HandleScrollZoom(float deltaTime);

private:
    Camera* camera_ = nullptr;

    // 設定
    float moveSpeed_ = 25.0f;
    float rotateSpeed_ = 1.4f;
    float scrollSpeed_ = 1.0f;

    // 状態
    bool viewportHovered_ = false;
    bool viewportFocused_ = false;
    bool isControlling_ = false;
    bool movementEnabled_ = true;  // WASD移動が有効か

    // マウス
    bool rightMousePressed_ = false;
    POINT lockMousePos_ = { 0, 0 };

    // フリーカメラ用
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;

    // オービット用
    Vector3 orbitTarget_ = Vector3::Zero();
    float orbitDistance_ = 5.0f;
    float orbitYaw_ = 0.0f;
    float orbitPitch_ = 0.0f;
    bool hasOrbitTarget_ = false;

    // ビューポート矩形
    struct { float x, y, w, h; } viewportRect_ = { 0, 0, 0, 0 };
};

} // namespace UnoEngine
