#pragma once

#include "CinematicSequence.h"
#include "CinematicPlayer.h"
#include "../Core/Camera.h"
#include <string>
#include <optional>

namespace UnoEngine {

// ============================================================
// CinematicEditor - ImGuiタイムラインエディタ
// Replay Mod風のカメラキーフレーム編集UIを提供する
// ============================================================
class CinematicEditor {
public:
    CinematicEditor() = default;

    // プレビュー先カメラ（スクラブ・再生時にこのカメラを操作する）
    void SetPreviewCamera(Camera* camera) { previewCamera_ = camera; }

    // シーンビューカメラ（Record時のポーズ取得元）
    void SetSceneViewCamera(Camera* camera) { sceneViewCamera_ = camera; }

    // 毎フレーム呼び出す（deltaTime 秒）
    void Update(float deltaTime);

    // ImGuiウィンドウを描画する（EditorUI::Render() から呼ぶ）
    void RenderWindow();

    // ウィンドウ表示状態
    bool IsOpen()   const { return isOpen_; }
    void SetOpen(bool v)  { isOpen_ = v; }
    void ToggleOpen()     { isOpen_ = !isOpen_; }

    // プレビュー再生中かどうか（エディタカメラ操作を外部で抑制するため）
    bool IsPreviewPlaying() const { return player_.IsPlaying(); }

    // 現在のシーケンスを取得（ゲームランタイムでの利用）
    const CinematicSequence& GetSequence() const { return sequence_; }

private:
    // ---- ImGui パネル ----
    void RenderToolbar();
    void RenderTimeline();
    void RenderKeyframeInspector();

    // ---- 操作ヘルパー ----
    void StampKeyframeFromCamera();      // 現在のシーンカメラからキーフレームを打刻
    void DeleteSelectedKeyframe();
    void SortKeyframes();
    void ApplyPreviewAt(float time);     // 指定時刻でプレビューカメラを更新

    // ---- 座標変換 ----
    float TimeToX(float t)    const;
    float XToTime(float x)    const;

private:
    CinematicSequence sequence_;
    CinematicPlayer   player_;

    Camera* previewCamera_   = nullptr;
    Camera* sceneViewCamera_ = nullptr;

    bool isOpen_ = false;

    // ---- タイムライン状態 ----
    float timelineZoom_    = 1.0f;   // ズーム倍率（1.0 = 80px/秒）
    float timelineScrollX_ = 0.0f;  // 水平スクロール量（秒）
    float scrubTime_       = 0.0f;  // スクラブヘッド位置（秒）

    // タイムライン描画領域（毎フレーム更新）
    float tlOriginX_ = 0.0f;        // タイムライン左端 X（スクリーン座標）
    float tlOriginY_ = 0.0f;        // タイムライン上端 Y（スクリーン座標）
    float tlWidth_   = 0.0f;        // タイムライン幅（px）

    bool isDraggingScrub_     = false;
    bool isDraggingKeyframe_  = false;
    float dragKeyframeOrigTime_ = 0.0f;

    // ---- キーフレーム選択 ----
    int selectedKeyframe_ = -1;    // -1 = 未選択

    // ---- レコードモード ----
    bool recordMode_ = false;

    // ---- バッファ ----
    char sequenceNameBuf_[128] = "Intro";
    char filePathBuf_[256]     = "Resources/cinematics/intro.json";

    // ---- 定数 ----
    static constexpr float kBasePixelsPerSec = 80.0f;
    static constexpr float kRulerHeight      = 22.0f;
    static constexpr float kTrackHeight      = 32.0f;
    static constexpr float kDiamondRadius    = 7.0f;
};

} // namespace UnoEngine
