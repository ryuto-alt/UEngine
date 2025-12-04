#pragma once

#include "../Core/Types.h"
#include "../Math/MathCommon.h"
#include <vector>
#include <algorithm>

namespace UnoEngine {

// カーブ用キーフレーム（ベジェカーブ対応）
// Note: AnimationClip.hのKeyframe<T>テンプレートとの衝突を避けるためCurveKeyframeと命名
struct CurveKeyframe {
    float time = 0.0f;          // 0-1 正規化時間
    float value = 0.0f;
    float inTangent = 0.0f;     // 入力タンジェント
    float outTangent = 0.0f;    // 出力タンジェント

    CurveKeyframe() = default;
    CurveKeyframe(float t, float v) : time(t), value(v), inTangent(0.0f), outTangent(0.0f) {}
    CurveKeyframe(float t, float v, float inT, float outT)
        : time(t), value(v), inTangent(inT), outTangent(outT) {}
};

// 補間モード
enum class CurveInterpolation {
    Linear,     // 線形補間
    Bezier,     // ベジェ補間
    Step        // ステップ（直前の値を維持）
};

// アニメーションカーブ
class AnimationCurve {
public:
    AnimationCurve() = default;

    // デフォルトカーブ（線形 0->1）
    static AnimationCurve Linear() {
        AnimationCurve curve;
        curve.AddKey(0.0f, 0.0f);
        curve.AddKey(1.0f, 1.0f);
        return curve;
    }

    // 定数カーブ
    static AnimationCurve Constant(float value) {
        AnimationCurve curve;
        curve.AddKey(0.0f, value);
        curve.AddKey(1.0f, value);
        return curve;
    }

    // EaseInOut
    static AnimationCurve EaseInOut() {
        AnimationCurve curve;
        curve.AddKey(CurveKeyframe(0.0f, 0.0f, 0.0f, 0.0f));
        curve.AddKey(CurveKeyframe(1.0f, 1.0f, 0.0f, 0.0f));
        curve.interpolation_ = CurveInterpolation::Bezier;
        return curve;
    }

    // キー追加
    void AddKey(float time, float value) {
        keys_.emplace_back(time, value);
        SortKeys();
    }

    void AddKey(const CurveKeyframe& key) {
        keys_.push_back(key);
        SortKeys();
    }

    // キー削除
    void RemoveKey(size_t index) {
        if (index < keys_.size()) {
            keys_.erase(keys_.begin() + index);
        }
    }

    // キー数取得
    size_t GetKeyCount() const { return keys_.size(); }

    // キー取得
    CurveKeyframe& GetKey(size_t index) { return keys_[index]; }
    const CurveKeyframe& GetKey(size_t index) const { return keys_[index]; }

    // 値を評価（t: 0-1）
    float Evaluate(float t) const {
        if (keys_.empty()) return 0.0f;
        if (keys_.size() == 1) return keys_[0].value;

        // 範囲外クランプ
        t = std::clamp(t, 0.0f, 1.0f);

        // 補間対象のキー検索
        size_t i = 0;
        for (; i < keys_.size() - 1; ++i) {
            if (t < keys_[i + 1].time) break;
        }

        if (i >= keys_.size() - 1) {
            return keys_.back().value;
        }

        const CurveKeyframe& k0 = keys_[i];
        const CurveKeyframe& k1 = keys_[i + 1];

        // ローカルt計算
        float dt = k1.time - k0.time;
        if (dt <= 0.0f) return k0.value;
        float localT = (t - k0.time) / dt;

        switch (interpolation_) {
        case CurveInterpolation::Linear:
            return Lerp(k0.value, k1.value, localT);

        case CurveInterpolation::Bezier:
            return EvaluateBezier(k0, k1, localT);

        case CurveInterpolation::Step:
            return k0.value;

        default:
            return Lerp(k0.value, k1.value, localT);
        }
    }

    // 補間モード設定
    void SetInterpolation(CurveInterpolation interp) { interpolation_ = interp; }
    CurveInterpolation GetInterpolation() const { return interpolation_; }

    // キーリスト取得（エディター用）
    std::vector<CurveKeyframe>& GetKeys() { return keys_; }
    const std::vector<CurveKeyframe>& GetKeys() const { return keys_; }

private:
    void SortKeys() {
        std::sort(keys_.begin(), keys_.end(),
            [](const CurveKeyframe& a, const CurveKeyframe& b) { return a.time < b.time; });
    }

    float Lerp(float a, float b, float t) const {
        return a + (b - a) * t;
    }

    // キュービックベジェ補間
    float EvaluateBezier(const CurveKeyframe& k0, const CurveKeyframe& k1, float t) const {
        float dt = k1.time - k0.time;
        float p0 = k0.value;
        float p1 = k0.value + k0.outTangent * dt / 3.0f;
        float p2 = k1.value - k1.inTangent * dt / 3.0f;
        float p3 = k1.value;

        // キュービックベジェ
        float u = 1.0f - t;
        return u * u * u * p0 +
               3.0f * u * u * t * p1 +
               3.0f * u * t * t * p2 +
               t * t * t * p3;
    }

private:
    std::vector<CurveKeyframe> keys_;
    CurveInterpolation interpolation_ = CurveInterpolation::Linear;
};

// カーブモード
enum class MinMaxCurveMode {
    Constant,       // 定数値
    Curve,          // カーブ
    RandomBetweenConstants,  // 2つの定数間でランダム
    RandomBetweenCurves      // 2つのカーブ間でランダム
};

// 最小最大カーブ（ランダム範囲対応）
struct MinMaxCurve {
    MinMaxCurveMode mode = MinMaxCurveMode::Constant;
    float constantMin = 0.0f;
    float constantMax = 1.0f;
    AnimationCurve curveMin;
    AnimationCurve curveMax;
    float curveMultiplier = 1.0f;  // カーブ値に掛ける係数

    MinMaxCurve() {
        curveMin = AnimationCurve::Linear();
        curveMax = AnimationCurve::Linear();
    }

    // 定数初期化
    static MinMaxCurve Constant(float value) {
        MinMaxCurve curve;
        curve.mode = MinMaxCurveMode::Constant;
        curve.constantMin = value;
        curve.constantMax = value;
        return curve;
    }

    // 範囲初期化
    static MinMaxCurve Range(float min, float max) {
        MinMaxCurve curve;
        curve.mode = MinMaxCurveMode::RandomBetweenConstants;
        curve.constantMin = min;
        curve.constantMax = max;
        return curve;
    }

    // 評価（t: ライフタイム進行率 0-1, random: パーティクル固有の乱数 0-1）
    float Evaluate(float t, float random = 0.5f) const {
        switch (mode) {
        case MinMaxCurveMode::Constant:
            return constantMin * curveMultiplier;

        case MinMaxCurveMode::Curve:
            return curveMin.Evaluate(t) * curveMultiplier;

        case MinMaxCurveMode::RandomBetweenConstants:
            return (constantMin + (constantMax - constantMin) * random) * curveMultiplier;

        case MinMaxCurveMode::RandomBetweenCurves: {
            float v0 = curveMin.Evaluate(t);
            float v1 = curveMax.Evaluate(t);
            return (v0 + (v1 - v0) * random) * curveMultiplier;
        }

        default:
            return constantMin * curveMultiplier;
        }
    }
};

} // namespace UnoEngine
