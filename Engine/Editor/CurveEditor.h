#pragma once

#include "../Core/Types.h"
#include "../Particle/Curve.h"
#include "../Particle/Gradient.h"
#include "../Particle/ParticleData.h"
#include <imgui.h>
#include <string>
#include <functional>

namespace UnoEngine {

// カーブエディター
class CurveEditor {
public:
    // 基本カーブエディター
    // 戻り値: 変更があった場合true
    static bool DrawCurve(const char* label, AnimationCurve& curve,
                          const ImVec2& size = ImVec2(200, 100),
                          float minValue = 0.0f, float maxValue = 1.0f);

    // MinMaxCurveエディター
    static bool DrawMinMaxCurve(const char* label, MinMaxCurve& curve,
                                float minValue = 0.0f, float maxValue = 1.0f);

    // グラデーションエディター
    static bool DrawGradient(const char* label, Gradient& gradient,
                            const ImVec2& size = ImVec2(200, 20));

    // MinMaxGradientエディター
    static bool DrawMinMaxGradient(const char* label, MinMaxGradient& gradient);

    // カラーピッカー（Float4）
    static bool ColorEdit4(const char* label, Float4& color);

    // カラーピッカー（Float3）
    static bool ColorEdit3(const char* label, Float3& color);

    // プリセットボタン
    static bool DrawCurvePresets(AnimationCurve& curve);

private:
    // 内部: キーフレームハンドル描画
    static void DrawKeyframeHandle(ImDrawList* drawList, const ImVec2& pos,
                                   bool selected, bool hovered);

    // 内部: ベジェカーブ描画
    static void DrawBezierCurve(ImDrawList* drawList, const AnimationCurve& curve,
                                const ImVec2& origin, const ImVec2& size,
                                float minValue, float maxValue, ImU32 color);

    // 内部: グラデーションバー描画
    static void DrawGradientBar(ImDrawList* drawList, const Gradient& gradient,
                               const ImVec2& pos, const ImVec2& size);

    // 選択中のキーインデックス（-1: 選択なし）
    static int selectedKeyIndex_;
    static int draggedKeyIndex_;
    static bool isDraggingTangent_;
    static bool isInTangent_;
};

// パーティクル用の特殊ウィジェット
class ParticleWidgets {
public:
    // 範囲スライダー（Min-Max）
    static bool RangeSlider(const char* label, float& minVal, float& maxVal,
                           float rangeMin = 0.0f, float rangeMax = 1.0f);

    // 3Dベクトル入力
    static bool Vector3Input(const char* label, Float3& vec);

    // 角度スライダー（度数法）
    static bool AngleSlider(const char* label, float& angleDegrees,
                           float minAngle = 0.0f, float maxAngle = 360.0f);

    // バースト設定エディター
    static bool BurstEditor(const char* label, std::vector<BurstConfig>& bursts);

    // 形状選択コンボボックス
    static bool ShapeCombo(const char* label, EmitShape& shape);

    // ブレンドモード選択
    static bool BlendModeCombo(const char* label, BlendMode& mode);

    // レンダリングモード選択
    static bool RenderModeCombo(const char* label, RenderMode& mode);
};

} // namespace UnoEngine
