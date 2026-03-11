#include "pch.h"
#include "CinematicPlayer.h"
#include <algorithm>
#include <cmath>

namespace UnoEngine {

static constexpr float kDeg2Rad = 3.14159265f / 180.0f;

// ============================================================
// Hermite 基底関数による三次補間
// P1→P2 を t∈[0,1] で補間、T1/T2 は各点での接線ベクトル
// ============================================================
static Vector3 HermiteVec3(const Vector3& p1, const Vector3& p2,
                           const Vector3& t1, const Vector3& t2, float t) {
    float t2v = t * t;
    float t3v = t2v * t;

    float h00 =  2.0f * t3v - 3.0f * t2v + 1.0f;
    float h10 =         t3v - 2.0f * t2v + t;
    float h01 = -2.0f * t3v + 3.0f * t2v;
    float h11 =         t3v -        t2v;

    return Vector3(
        h00 * p1.GetX() + h10 * t1.GetX() + h01 * p2.GetX() + h11 * t2.GetX(),
        h00 * p1.GetY() + h10 * t1.GetY() + h01 * p2.GetY() + h11 * t2.GetY(),
        h00 * p1.GetZ() + h10 * t1.GetZ() + h01 * p2.GetZ() + h11 * t2.GetZ()
    );
}

// ============================================================
// イージング関数
// ============================================================
float CinematicPlayer::ApplyEasing(float t, CameraKeyframe::Easing easing) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (easing) {
        case CameraKeyframe::Easing::Linear:
            return t;
        case CameraKeyframe::Easing::EaseIn:
            return t * t * t;
        case CameraKeyframe::Easing::EaseOut: {
            float u = 1.0f - t;
            return 1.0f - u * u * u;
        }
        case CameraKeyframe::Easing::EaseInOut: {
            return t * t * (3.0f - 2.0f * t);
        }
        case CameraKeyframe::Easing::Emphasis: {
            float t3 = t * t * t;
            return t3 * (t * (t * 6.0f - 15.0f) + 10.0f);
        }
    }
    return t;
}

// ============================================================
// SetSequence — スプライン接線を事前計算
// ============================================================
void CinematicPlayer::SetSequence(const CinematicSequence& seq) {
    sequence_ = seq;
    RebuildSplineTangents();
}

// ============================================================
// 自然三次スプラインの接線を三重対角行列で求解（C2連続）
// 各セグメントの接線をスケーリングして区間[0,1]のHermiteに対応させる
// ============================================================
void CinematicPlayer::RebuildSplineTangents() {
    const auto& kfs = sequence_.keyframes;
    size_t n = kfs.size();
    splineTangents_.clear();

    if (n < 2) {
        splineTangents_.resize(n, Vector3::Zero());
        return;
    }

    // セグメント長 h[i] = time[i+1] - time[i]
    std::vector<float> h(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        h[i] = std::max(kfs[i + 1].time - kfs[i].time, 0.001f);
    }

    // 三重対角法で各キーフレームの接線（微分値）を求解
    // 条件: 隣接セグントの2階微分が境界で一致 → C2連続
    // 自然スプライン端点条件: 2階微分 = 0
    //
    // 内部点 i (1..n-2):
    //   h[i-1]*D[i-1] + 2*(h[i-1]+h[i])*D[i] + h[i]*D[i+1]
    //     = 3*( h[i-1]/h[i]*(P[i+1]-P[i]) + h[i]/h[i-1]*(P[i]-P[i-1]) )
    //
    // 端点（自然スプライン）:
    //   2*D[0] + D[1] = 3*(P[1]-P[0])/h[0]
    //   D[n-2] + 2*D[n-1] = 3*(P[n-1]-P[n-2])/h[n-2]

    // XYZ を個別にトーマス法で解く
    splineTangents_.resize(n);

    for (int axis = 0; axis < 3; ++axis) {
        auto getVal = [&](size_t idx) -> float {
            const auto& p = kfs[idx].position;
            if (axis == 0) return p.GetX();
            if (axis == 1) return p.GetY();
            return p.GetZ();
        };

        // 係数配列 (a,b,c,d) for tridiagonal system
        std::vector<float> a(n), b(n), c(n), d(n);

        // 端点 0
        b[0] = 2.0f;
        c[0] = 1.0f;
        d[0] = 3.0f * (getVal(1) - getVal(0)) / h[0];

        // 内部点
        for (size_t i = 1; i < n - 1; ++i) {
            a[i] = h[i - 1];
            b[i] = 2.0f * (h[i - 1] + h[i]);
            c[i] = h[i];
            d[i] = 3.0f * (
                h[i - 1] / h[i] * (getVal(i + 1) - getVal(i)) +
                h[i] / h[i - 1] * (getVal(i) - getVal(i - 1))
            );
        }

        // 端点 n-1
        a[n - 1] = 1.0f;
        b[n - 1] = 2.0f;
        d[n - 1] = 3.0f * (getVal(n - 1) - getVal(n - 2)) / h[n - 2];

        // トーマス法（前進消去）
        for (size_t i = 1; i < n; ++i) {
            float w = a[i] / b[i - 1];
            b[i] -= w * c[i - 1];
            d[i] -= w * d[i - 1];
        }

        // 後退代入
        std::vector<float> D(n);
        D[n - 1] = d[n - 1] / b[n - 1];
        for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
            D[i] = (d[i] - c[i] * D[i + 1]) / b[i];
        }

        // 接線を格納（Hermite用にセグメント長 h[i] でスケーリング）
        for (size_t i = 0; i < n; ++i) {
            float scaled = D[i];
            // Hermiteはt∈[0,1]なのでセグメント長分スケール
            if (i < n - 1) {
                scaled *= h[i];
            } else {
                scaled *= h[n - 2];
            }
            auto& tgt = splineTangents_[i];
            if (axis == 0) tgt = Vector3(scaled, tgt.GetY(), tgt.GetZ());
            else if (axis == 1) tgt = Vector3(tgt.GetX(), scaled, tgt.GetZ());
            else tgt = Vector3(tgt.GetX(), tgt.GetY(), scaled);
        }
    }
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
// EvaluateAt — 自然三次スプライン(C2) + Slerp回転
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

    // 区間インデックスを探す
    size_t i = 0;
    for (; i < kfs.size() - 1; ++i) {
        if (time < kfs[i + 1].time) break;
    }

    size_t i1 = i;
    size_t i2 = i + 1;

    const CameraKeyframe& k1 = kfs[i1];
    const CameraKeyframe& k2 = kfs[i2];

    float dt = k2.time - k1.time;
    float rawT = (dt > 0.0f) ? (time - k1.time) / dt : 0.0f;
    rawT = std::clamp(rawT, 0.0f, 1.0f);

    float easedT = ApplyEasing(rawT, k2.easing);

    // ---- 位置: 自然三次スプライン（C2連続）----
    if (splineTangents_.size() == kfs.size()) {
        outPos = HermiteVec3(k1.position, k2.position,
                             splineTangents_[i1], splineTangents_[i2], rawT);
    } else {
        // フォールバック: 線形補間
        outPos = Vector3(
            k1.position.GetX() + (k2.position.GetX() - k1.position.GetX()) * rawT,
            k1.position.GetY() + (k2.position.GetY() - k1.position.GetY()) * rawT,
            k1.position.GetZ() + (k2.position.GetZ() - k1.position.GetZ()) * rawT
        );
    }

    // ---- 回転: Slerp（イージング適用）----
    outRot = Quaternion::Slerp(k1.rotation, k2.rotation, easedT);

    // ---- FOV: 線形補間（イージング適用）----
    outFov = k1.fov + (k2.fov - k1.fov) * easedT;
}

} // namespace UnoEngine
