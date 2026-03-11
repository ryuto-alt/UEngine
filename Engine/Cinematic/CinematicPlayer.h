#pragma once

#include "CinematicSequence.h"
#include "../Core/Camera.h"
#include <vector>

namespace UnoEngine {

// ============================================================
// CinematicPlayer - シネマティックシーケンス再生エンジン
// カメラを自動操作してイントロ演出などを再生する
// ============================================================
class CinematicPlayer {
public:
    CinematicPlayer() = default;

    void SetSequence(const CinematicSequence& seq);
    const CinematicSequence& GetSequence() const   { return sequence_; }
    void SetCamera(Camera* camera)                  { camera_ = camera; }

    // 再生制御
    void Play();
    void Pause();
    void Stop();
    void Seek(float time);

    // 毎フレーム更新（Play中のみカメラを動かす）
    void Update(float deltaTime);

    // 状態
    bool  IsPlaying()   const { return isPlaying_; }
    bool  IsFinished()  const { return isFinished_; }
    float GetCurrentTime() const { return currentTime_; }
    float GetDuration()    const { return sequence_.duration; }

    // 任意時刻でのポーズ評価（プレビュー・スクラブ用）
    void EvaluateAt(float time, Vector3& outPos, Quaternion& outRot, float& outFov) const;

    // イベント評価
    std::vector<const CinematicEvent*> GetActiveEvents(float time) const;
    bool IsWaitingForInput() const { return waitingForInput_; }
    void ResolveWaitForInput();

    // 現在アクティブなテキストイベントのフェードアルファを計算
    static float ComputeEventAlpha(const CinematicEvent& ev, float currentTime);

private:
    static float ApplyEasing(float t, CameraKeyframe::Easing easing);
    void ApplyToCamera(float time);
    void RebuildSplineTangents();

private:
    CinematicSequence sequence_;
    Camera*           camera_      = nullptr;
    float             currentTime_ = 0.0f;
    bool              isPlaying_   = false;
    bool              isFinished_  = false;

    // 自然三次スプライン用の事前計算済み接線（C2連続）
    std::vector<Vector3> splineTangents_;

    // イベント再生状態
    bool  waitingForInput_ = false;
    int   lastFiredEventIndex_ = -1;   // ワンショットイベント用
    std::vector<bool> eventFired_;     // Audio/Lua/ObjectToggle は一度だけ発火
};

} // namespace UnoEngine
