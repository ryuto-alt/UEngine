#pragma once

#include "../Core/Types.h"
#include "../Math/MathCommon.h"
#include <vector>
#include <algorithm>

namespace UnoEngine {

// 色キー
struct GradientColorKey {
    Float3 color = { 1.0f, 1.0f, 1.0f };
    float time = 0.0f;  // 0-1

    GradientColorKey() = default;
    GradientColorKey(const Float3& c, float t) : color(c), time(t) {}
};

// アルファキー
struct GradientAlphaKey {
    float alpha = 1.0f;
    float time = 0.0f;  // 0-1

    GradientAlphaKey() = default;
    GradientAlphaKey(float a, float t) : alpha(a), time(t) {}
};

// カラーグラデーション
class Gradient {
public:
    Gradient() {
        // デフォルト: 白から白
        colorKeys_.emplace_back(Float3{ 1.0f, 1.0f, 1.0f }, 0.0f);
        colorKeys_.emplace_back(Float3{ 1.0f, 1.0f, 1.0f }, 1.0f);
        alphaKeys_.emplace_back(1.0f, 0.0f);
        alphaKeys_.emplace_back(1.0f, 1.0f);
    }

    // プリセット: 白->透明
    static Gradient FadeOut() {
        Gradient grad;
        grad.colorKeys_.clear();
        grad.alphaKeys_.clear();
        grad.colorKeys_.emplace_back(Float3{ 1.0f, 1.0f, 1.0f }, 0.0f);
        grad.colorKeys_.emplace_back(Float3{ 1.0f, 1.0f, 1.0f }, 1.0f);
        grad.alphaKeys_.emplace_back(1.0f, 0.0f);
        grad.alphaKeys_.emplace_back(0.0f, 1.0f);
        return grad;
    }

    // プリセット: 炎
    static Gradient Fire() {
        Gradient grad;
        grad.colorKeys_.clear();
        grad.alphaKeys_.clear();
        grad.colorKeys_.emplace_back(Float3{ 1.0f, 1.0f, 0.8f }, 0.0f);   // 白黄
        grad.colorKeys_.emplace_back(Float3{ 1.0f, 0.6f, 0.0f }, 0.3f);   // オレンジ
        grad.colorKeys_.emplace_back(Float3{ 1.0f, 0.2f, 0.0f }, 0.6f);   // 赤
        grad.colorKeys_.emplace_back(Float3{ 0.2f, 0.0f, 0.0f }, 1.0f);   // 暗い赤
        grad.alphaKeys_.emplace_back(0.0f, 0.0f);
        grad.alphaKeys_.emplace_back(1.0f, 0.1f);
        grad.alphaKeys_.emplace_back(1.0f, 0.5f);
        grad.alphaKeys_.emplace_back(0.0f, 1.0f);
        return grad;
    }

    // 色キー追加
    void AddColorKey(const Float3& color, float time) {
        colorKeys_.emplace_back(color, time);
        SortColorKeys();
    }

    // アルファキー追加
    void AddAlphaKey(float alpha, float time) {
        alphaKeys_.emplace_back(alpha, time);
        SortAlphaKeys();
    }

    // 色キー削除
    void RemoveColorKey(size_t index) {
        if (index < colorKeys_.size() && colorKeys_.size() > 1) {
            colorKeys_.erase(colorKeys_.begin() + index);
        }
    }

    // アルファキー削除
    void RemoveAlphaKey(size_t index) {
        if (index < alphaKeys_.size() && alphaKeys_.size() > 1) {
            alphaKeys_.erase(alphaKeys_.begin() + index);
        }
    }

    // 評価（t: 0-1）
    Float4 Evaluate(float t) const {
        Float3 color = EvaluateColor(t);
        float alpha = EvaluateAlpha(t);
        return Float4{ color.x, color.y, color.z, alpha };
    }

    // 色のみ評価
    Float3 EvaluateColor(float t) const {
        if (colorKeys_.empty()) return Float3{ 1.0f, 1.0f, 1.0f };
        if (colorKeys_.size() == 1) return colorKeys_[0].color;

        t = std::clamp(t, 0.0f, 1.0f);

        // キー検索
        size_t i = 0;
        for (; i < colorKeys_.size() - 1; ++i) {
            if (t < colorKeys_[i + 1].time) break;
        }

        if (i >= colorKeys_.size() - 1) {
            return colorKeys_.back().color;
        }

        const auto& k0 = colorKeys_[i];
        const auto& k1 = colorKeys_[i + 1];

        float dt = k1.time - k0.time;
        if (dt <= 0.0f) return k0.color;
        float localT = (t - k0.time) / dt;

        return LerpColor(k0.color, k1.color, localT);
    }

    // アルファのみ評価
    float EvaluateAlpha(float t) const {
        if (alphaKeys_.empty()) return 1.0f;
        if (alphaKeys_.size() == 1) return alphaKeys_[0].alpha;

        t = std::clamp(t, 0.0f, 1.0f);

        // キー検索
        size_t i = 0;
        for (; i < alphaKeys_.size() - 1; ++i) {
            if (t < alphaKeys_[i + 1].time) break;
        }

        if (i >= alphaKeys_.size() - 1) {
            return alphaKeys_.back().alpha;
        }

        const auto& k0 = alphaKeys_[i];
        const auto& k1 = alphaKeys_[i + 1];

        float dt = k1.time - k0.time;
        if (dt <= 0.0f) return k0.alpha;
        float localT = (t - k0.time) / dt;

        return k0.alpha + (k1.alpha - k0.alpha) * localT;
    }

    // キーリスト取得（エディター用）
    std::vector<GradientColorKey>& GetColorKeys() { return colorKeys_; }
    const std::vector<GradientColorKey>& GetColorKeys() const { return colorKeys_; }
    std::vector<GradientAlphaKey>& GetAlphaKeys() { return alphaKeys_; }
    const std::vector<GradientAlphaKey>& GetAlphaKeys() const { return alphaKeys_; }

private:
    void SortColorKeys() {
        std::sort(colorKeys_.begin(), colorKeys_.end(),
            [](const GradientColorKey& a, const GradientColorKey& b) { return a.time < b.time; });
    }

    void SortAlphaKeys() {
        std::sort(alphaKeys_.begin(), alphaKeys_.end(),
            [](const GradientAlphaKey& a, const GradientAlphaKey& b) { return a.time < b.time; });
    }

    Float3 LerpColor(const Float3& a, const Float3& b, float t) const {
        return Float3{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

private:
    std::vector<GradientColorKey> colorKeys_;
    std::vector<GradientAlphaKey> alphaKeys_;
};

// グラデーションモード
enum class MinMaxGradientMode {
    Constant,       // 単色
    Gradient,       // グラデーション
    RandomBetweenColors,    // 2色間でランダム
    RandomBetweenGradients  // 2グラデーション間でランダム
};

// 最小最大グラデーション
struct MinMaxGradient {
    MinMaxGradientMode mode = MinMaxGradientMode::Constant;
    Float4 colorMin = { 1.0f, 1.0f, 1.0f, 1.0f };
    Float4 colorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
    Gradient gradientMin;
    Gradient gradientMax;

    MinMaxGradient() = default;

    // 単色初期化
    static MinMaxGradient Color(const Float4& color) {
        MinMaxGradient grad;
        grad.mode = MinMaxGradientMode::Constant;
        grad.colorMin = color;
        grad.colorMax = color;
        return grad;
    }

    // グラデーション初期化
    static MinMaxGradient FromGradient(const Gradient& gradient) {
        MinMaxGradient grad;
        grad.mode = MinMaxGradientMode::Gradient;
        grad.gradientMin = gradient;
        return grad;
    }

    // 評価
    Float4 Evaluate(float t, float random = 0.5f) const {
        switch (mode) {
        case MinMaxGradientMode::Constant:
            return colorMin;

        case MinMaxGradientMode::Gradient:
            return gradientMin.Evaluate(t);

        case MinMaxGradientMode::RandomBetweenColors:
            return LerpColor(colorMin, colorMax, random);

        case MinMaxGradientMode::RandomBetweenGradients: {
            Float4 c0 = gradientMin.Evaluate(t);
            Float4 c1 = gradientMax.Evaluate(t);
            return LerpColor(c0, c1, random);
        }

        default:
            return colorMin;
        }
    }

private:
    Float4 LerpColor(const Float4& a, const Float4& b, float t) const {
        return Float4{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        };
    }
};

} // namespace UnoEngine
