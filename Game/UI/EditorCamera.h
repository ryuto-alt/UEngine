#pragma once

#include "../../Engine/Core/Camera.h"


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

    // カメラ操作中かどうか
    bool IsControlling() const { return isControlling_; }

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
    float moveSpeed_ = 5.0f;
    float rotateSpeed_ = 0.3f;
    float scrollSpeed_ = 1.0f;

    // 状態
    bool viewportHovered_ = false;
    bool viewportFocused_ = false;
    bool isControlling_ = false;

    // マウス位置
    float lastMousePosX_ = 0.0f;
    float lastMousePosY_ = 0.0f;
    bool rightMousePressed_ = false;
};

} // namespace UnoEngine
