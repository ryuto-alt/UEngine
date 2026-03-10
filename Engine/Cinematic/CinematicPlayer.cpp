#include "pch.h"
#include "CinematicPlayer.h"
#include <algorithm>
#include <cmath>

namespace UnoEngine {

static constexpr float kDeg2Rad = 3.14159265f / 180.0f;

// ============================================================
// Catmull-Rom スプライン補間（スカラー）
// p0,p1,p2,p3 : 4点、t : [0,1] で p1→p2 区間を補間
// ============================================================
static float CatmullRomScalar(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2)                        * t  +
        (2.0f*p0 - 5.0f*p1 + 4.0f*p2 - p3) * t2 +
        (-p0 + 3.0f*p1 - 3.0f*p2 + p3)   * t3
    );
}

static Vector3 CatmullRomVec3(const Vector3& p0, const Vector3& p1,
                               const Vector3& p2, const Vector3& p3, float t) {
    return Vector3(
        CatmullRomScalar(p0.GetX(), p1.GetX(), p2.GetX(), p3.GetX(), t),
        CatmullRomScalar(p0.GetY(), p1.GetY(), p2.GetY(), p3.GetY(), t),
        CatmullRomScalar(p0.GetZ(), p1.GetZ(), p2.GetZ(), p3.GetZ(), t)
    );
}

// ============================================================
// 再生制御
// ============================================================
void CinematicPlayer::Play() {
    if (isFinished_) {
        currentTime_ = 0.0f;
        isFinished_  = false;
    }
    isPlaying_ = true;
}

void CinematicPlayer::Pause() {
    isPlaying_ = false;
}

void CinematicPlayer::Stop() {
    isPlaying_   = false;
    isFinished_  = false;
    currentTime_ = 0.0f;
    if (camera_ && !sequence_.keyframes.empty()) {
        ApplyToCamera(0.0f);
    }
}

void CinematicPlayer::Seek(float time) {
    currentTime_ = std::clamp(time, 0.0f, sequence_.duration);
    isFinished_  = false;
    if (camera_ && !sequence_.keyframes.empty()) {
        ApplyToCamera(currentTime_);
    }
}

void CinematicPlayer::Update(float deltaTime) {
    if (!isPlaying_ || !camera_) return;
    if (sequence_.keyframes.size() < 1) return;

    currentTime_ += deltaTime;

    if (currentTime_ >= sequence_.duration) {
        if (sequence_.loop) {
            currentTime_ = std::fmod(currentTime_, sequence_.duration);
        } else {
            currentTime_ = sequence_.duration;
            isPlaying_   = false;
            isFinished_  = true;
        }
    }

    ApplyToCamera(currentTime_);
}

// ============================================================
// カメラへ適用
// NOTE: CameraKeyframe.fov は度数法で保持
//       Camera::SetPerspective はラジアンを要求するため変換する
// ============================================================
void CinematicPlayer::ApplyToCamera(float time) {
    Vector3    pos;
    Quaternion rot;
    float      fovDeg = 60.0f;
    EvaluateAt(time, pos, rot, fovDeg);
    camera_->SetPosition(pos);
    camera_->SetRotation(rot);
    camera_->SetPerspective(fovDeg * kDeg2Rad,
                            camera_->GetAspectRatio(),
                            camera_->GetNearClip(),
                            camera_->GetFarClip());
}

// ============================================================
// EvaluateAt - Catmull-Rom による位置補間 + Slerp 回転補間
// ============================================================
void CinematicPlayer::EvaluateAt(float time,
                                  Vector3&    outPos,
                                  Quaternion& outRot,
                                  float&      outFov) const {
    const auto& kfs = sequence_.keyframes;

    if (kfs.empty()) {
        outPos = Vector3::Zero();
        outRot = Quaternion();
        outFov = 60.0f;
        return;
    }

    if (kfs.size() == 1 || time <= kfs.front().time) {
        outPos = kfs.front().position;
        outRot = kfs.front().rotation;
        outFov = kfs.front().fov;
        return;
    }

    if (time >= kfs.back().time) {
        outPos = kfs.back().position;
        outRot = kfs.back().rotation;
        outFov = kfs.back().fov;
        return;
    }

    // 区間インデックスを探す（k1 → k2 の区間）
    size_t i = 0;
    for (; i < kfs.size() - 1; ++i) {
        if (time < kfs[i + 1].time) break;
    }

    // 4点取得（端は折り返し）
    size_t n    = kfs.size();
    size_t i0   = (i > 0)     ? i - 1 : i;        // P0
    size_t i1   = i;                                // P1
    size_t i2   = i + 1;                            // P2
    size_t i3   = (i + 2 < n) ? i + 2 : i + 1;    // P3

    const CameraKeyframe& k0 = kfs[i0];
    const CameraKeyframe& k1 = kfs[i1];
    const CameraKeyframe& k2 = kfs[i2];
    const CameraKeyframe& k3 = kfs[i3];

    // ローカル補間パラメータ t ∈ [0,1]
    float dt = k2.time - k1.time;
    float t  = (dt > 0.0f) ? (time - k1.time) / dt : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    // ---- 位置: Catmull-Rom スプライン ----
    outPos = CatmullRomVec3(k0.position, k1.position, k2.position, k3.position, t);

    // ---- 回転: 球面線形補間（Slerp）----
    outRot = Quaternion::Slerp(k1.rotation, k2.rotation, t);

    // ---- FOV: 線形補間 ----
    outFov = k1.fov + (k2.fov - k1.fov) * t;
}

} // namespace UnoEngine
