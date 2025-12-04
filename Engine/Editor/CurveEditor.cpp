// UTF-8 BOM: This file should be saved with UTF-8 encoding
#include "pch.h"
#include "CurveEditor.h"
#include <algorithm>
#include <cmath>

namespace UnoEngine {

int CurveEditor::selectedKeyIndex_ = -1;
int CurveEditor::draggedKeyIndex_ = -1;
bool CurveEditor::isDraggingTangent_ = false;
bool CurveEditor::isInTangent_ = false;

bool CurveEditor::DrawCurve(const char* label, AnimationCurve& curve,
                            const ImVec2& size, float minValue, float maxValue) {
    bool changed = false;

    ImGui::PushID(label);

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = size;

    // 背景
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(40, 40, 40, 255));
    drawList->AddRect(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(80, 80, 80, 255));

    // グリッド
    int gridLines = 4;
    for (int i = 1; i < gridLines; ++i) {
        float x = canvasPos.x + canvasSize.x * i / gridLines;
        float y = canvasPos.y + canvasSize.y * i / gridLines;
        drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y),
            IM_COL32(60, 60, 60, 255));
        drawList->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y),
            IM_COL32(60, 60, 60, 255));
    }

    // カーブ描画
    DrawBezierCurve(drawList, curve, canvasPos, canvasSize, minValue, maxValue,
        IM_COL32(100, 200, 100, 255));

    // キーフレーム描画とインタラクション
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("curve_canvas", canvasSize);

    bool isHovered = ImGui::IsItemHovered();
    bool isClicked = ImGui::IsItemClicked(0);
    bool isRightClicked = ImGui::IsItemClicked(1);
    ImVec2 mousePos = ImGui::GetMousePos();

    // マウス位置をカーブ座標に変換
    auto screenToValue = [&](const ImVec2& screen) -> ImVec2 {
        float t = (screen.x - canvasPos.x) / canvasSize.x;
        float v = 1.0f - (screen.y - canvasPos.y) / canvasSize.y;
        v = minValue + v * (maxValue - minValue);
        return ImVec2(t, v);
    };

    auto valueToScreen = [&](float t, float v) -> ImVec2 {
        float normalizedV = (v - minValue) / (maxValue - minValue);
        return ImVec2(
            canvasPos.x + t * canvasSize.x,
            canvasPos.y + (1.0f - normalizedV) * canvasSize.y
        );
    };

    // キーフレームのドラッグ処理
    auto& keys = curve.GetKeys();
    int hoveredKey = -1;

    for (size_t i = 0; i < keys.size(); ++i) {
        ImVec2 keyPos = valueToScreen(keys[i].time, keys[i].value);
        float dist = std::sqrt(
            (mousePos.x - keyPos.x) * (mousePos.x - keyPos.x) +
            (mousePos.y - keyPos.y) * (mousePos.y - keyPos.y)
        );

        if (dist < 8.0f) {
            hoveredKey = static_cast<int>(i);
        }

        bool isSelected = (selectedKeyIndex_ == static_cast<int>(i));
        DrawKeyframeHandle(drawList, keyPos, isSelected, hoveredKey == static_cast<int>(i));

        // ベジェハンドル描画（選択時）
        if (isSelected && curve.GetInterpolation() == CurveInterpolation::Bezier) {
            // In Tangent
            ImVec2 inHandle = valueToScreen(
                keys[i].time - 0.1f,
                keys[i].value - keys[i].inTangent * 0.1f
            );
            drawList->AddLine(keyPos, inHandle, IM_COL32(150, 150, 255, 255), 1.5f);
            drawList->AddCircleFilled(inHandle, 4.0f, IM_COL32(150, 150, 255, 255));

            // Out Tangent
            ImVec2 outHandle = valueToScreen(
                keys[i].time + 0.1f,
                keys[i].value + keys[i].outTangent * 0.1f
            );
            drawList->AddLine(keyPos, outHandle, IM_COL32(255, 150, 150, 255), 1.5f);
            drawList->AddCircleFilled(outHandle, 4.0f, IM_COL32(255, 150, 150, 255));
        }
    }

    // クリックでキー選択
    if (isClicked) {
        if (hoveredKey >= 0) {
            selectedKeyIndex_ = hoveredKey;
            draggedKeyIndex_ = hoveredKey;
        } else {
            // ダブルクリックで新規キー追加
            if (ImGui::IsMouseDoubleClicked(0)) {
                ImVec2 value = screenToValue(mousePos);
                value.x = std::clamp(value.x, 0.0f, 1.0f);
                value.y = std::clamp(value.y, minValue, maxValue);
                curve.AddKey(value.x, value.y);
                changed = true;
            }
            selectedKeyIndex_ = -1;
        }
    }

    // 右クリックでキー削除
    if (isRightClicked && hoveredKey >= 0 && keys.size() > 2) {
        curve.RemoveKey(hoveredKey);
        selectedKeyIndex_ = -1;
        changed = true;
    }

    // ドラッグでキー移動
    if (draggedKeyIndex_ >= 0 && static_cast<size_t>(draggedKeyIndex_) < keys.size() && ImGui::IsMouseDragging(0)) {
        ImVec2 value = screenToValue(mousePos);
        value.x = std::clamp(value.x, 0.0f, 1.0f);
        value.y = std::clamp(value.y, minValue, maxValue);

        keys[draggedKeyIndex_].time = value.x;
        keys[draggedKeyIndex_].value = value.y;
        changed = true;
    }

    if (ImGui::IsMouseReleased(0)) {
        draggedKeyIndex_ = -1;
    }

    ImGui::PopID();

    // ラベル
    ImGui::SameLine();
    ImGui::Text("%s", label);

    return changed;
}

bool CurveEditor::DrawMinMaxCurve(const char* label, MinMaxCurve& curve,
                                   float minValue, float maxValue) {
    bool changed = false;

    ImGui::PushID(label);

    // モード選択
    const char* modeNames[] = { "定数", "カーブ", "定数範囲", "カーブ範囲" };
    int currentMode = static_cast<int>(curve.mode);
    if (ImGui::Combo("モード###Mode", &currentMode, modeNames, IM_ARRAYSIZE(modeNames))) {
        curve.mode = static_cast<MinMaxCurveMode>(currentMode);
        changed = true;
    }

    switch (curve.mode) {
    case MinMaxCurveMode::Constant:
        if (ImGui::DragFloat("値###Value", &curve.constantMin, 0.01f, minValue, maxValue)) {
            curve.constantMax = curve.constantMin;
            changed = true;
        }
        break;

    case MinMaxCurveMode::Curve:
        changed |= DrawCurve("カーブ###Curve", curve.curveMin, ImVec2(200, 80), minValue, maxValue);
        break;

    case MinMaxCurveMode::RandomBetweenConstants:
        changed |= ImGui::DragFloat("最小###Min", &curve.constantMin, 0.01f, minValue, maxValue);
        changed |= ImGui::DragFloat("最大###Max", &curve.constantMax, 0.01f, minValue, maxValue);
        break;

    case MinMaxCurveMode::RandomBetweenCurves:
        ImGui::Text("最小カーブ:");
        changed |= DrawCurve("##MinCurve", curve.curveMin, ImVec2(200, 60), minValue, maxValue);
        ImGui::Text("最大カーブ:");
        changed |= DrawCurve("##MaxCurve", curve.curveMax, ImVec2(200, 60), minValue, maxValue);
        break;
    }

    // 乗算値
    if (ImGui::DragFloat("乗算###Mult", &curve.curveMultiplier, 0.01f, 0.0f, 10.0f)) {
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

bool CurveEditor::DrawGradient(const char* label, Gradient& gradient, const ImVec2& size) {
    bool changed = false;

    ImGui::PushID(label);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // グラデーションバー描画
    DrawGradientBar(drawList, gradient, pos, size);

    // インタラクション用の透明ボタン
    ImGui::InvisibleButton("gradient_bar", size);

    bool isHovered = ImGui::IsItemHovered();
    ImVec2 mousePos = ImGui::GetMousePos();
    float mouseT = (mousePos.x - pos.x) / size.x;
    mouseT = std::clamp(mouseT, 0.0f, 1.0f);

    // ダブルクリックで色キー追加
    if (isHovered && ImGui::IsMouseDoubleClicked(0)) {
        Float3 color = gradient.EvaluateColor(mouseT);
        gradient.AddColorKey(color, mouseT);
        changed = true;
    }

    // 色キーマーカー描画
    auto& colorKeys = gradient.GetColorKeys();
    for (size_t i = 0; i < colorKeys.size(); ++i) {
        float x = pos.x + colorKeys[i].time * size.x;
        float y = pos.y + size.y;

        // 三角形マーカー
        drawList->AddTriangleFilled(
            ImVec2(x, y),
            ImVec2(x - 5, y + 8),
            ImVec2(x + 5, y + 8),
            IM_COL32(
                static_cast<int>(colorKeys[i].color.x * 255),
                static_cast<int>(colorKeys[i].color.y * 255),
                static_cast<int>(colorKeys[i].color.z * 255),
                255
            )
        );
        drawList->AddTriangle(
            ImVec2(x, y),
            ImVec2(x - 5, y + 8),
            ImVec2(x + 5, y + 8),
            IM_COL32(255, 255, 255, 255)
        );
    }

    // アルファキーマーカー描画（上側）
    auto& alphaKeys = gradient.GetAlphaKeys();
    for (size_t i = 0; i < alphaKeys.size(); ++i) {
        float x = pos.x + alphaKeys[i].time * size.x;
        float y = pos.y;

        int alpha = static_cast<int>(alphaKeys[i].alpha * 255);
        drawList->AddTriangleFilled(
            ImVec2(x, y),
            ImVec2(x - 5, y - 8),
            ImVec2(x + 5, y - 8),
            IM_COL32(alpha, alpha, alpha, 255)
        );
        drawList->AddTriangle(
            ImVec2(x, y),
            ImVec2(x - 5, y - 8),
            ImVec2(x + 5, y - 8),
            IM_COL32(255, 255, 255, 255)
        );
    }

    ImGui::PopID();

    ImGui::SameLine();
    ImGui::Text("%s", label);

    return changed;
}

bool CurveEditor::DrawMinMaxGradient(const char* label, MinMaxGradient& gradient) {
    bool changed = false;

    ImGui::PushID(label);

    const char* modeNames[] = { "単色", "グラデーション", "ランダム色", "ランダムグラデーション" };
    int currentMode = static_cast<int>(gradient.mode);
    if (ImGui::Combo("モード###GradMode", &currentMode, modeNames, IM_ARRAYSIZE(modeNames))) {
        gradient.mode = static_cast<MinMaxGradientMode>(currentMode);
        changed = true;
    }

    switch (gradient.mode) {
    case MinMaxGradientMode::Constant:
        changed |= ColorEdit4("カラー###Color", gradient.colorMin);
        break;

    case MinMaxGradientMode::Gradient:
        changed |= DrawGradient("グラデーション###Grad", gradient.gradientMin);
        break;

    case MinMaxGradientMode::RandomBetweenColors:
        changed |= ColorEdit4("カラー最小###ColorMin", gradient.colorMin);
        changed |= ColorEdit4("カラー最大###ColorMax", gradient.colorMax);
        break;

    case MinMaxGradientMode::RandomBetweenGradients:
        ImGui::Text("グラデーション最小:");
        changed |= DrawGradient("##GradMin", gradient.gradientMin);
        ImGui::Text("グラデーション最大:");
        changed |= DrawGradient("##GradMax", gradient.gradientMax);
        break;
    }

    ImGui::PopID();
    return changed;
}

bool CurveEditor::ColorEdit4(const char* label, Float4& color) {
    float c[4] = { color.x, color.y, color.z, color.w };
    if (ImGui::ColorEdit4(label, c)) {
        color = { c[0], c[1], c[2], c[3] };
        return true;
    }
    return false;
}

bool CurveEditor::ColorEdit3(const char* label, Float3& color) {
    float c[3] = { color.x, color.y, color.z };
    if (ImGui::ColorEdit3(label, c)) {
        color = { c[0], c[1], c[2] };
        return true;
    }
    return false;
}

bool CurveEditor::DrawCurvePresets(AnimationCurve& curve) {
    bool changed = false;

    if (ImGui::Button("線形###Linear")) {
        curve = AnimationCurve::Linear();
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("イーズ###Ease")) {
        curve = AnimationCurve::EaseInOut();
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("定数###Const")) {
        curve = AnimationCurve::Constant(1.0f);
        changed = true;
    }

    return changed;
}

void CurveEditor::DrawKeyframeHandle(ImDrawList* drawList, const ImVec2& pos,
                                      bool selected, bool hovered) {
    ImU32 color = selected ? IM_COL32(255, 200, 50, 255) :
                  hovered ? IM_COL32(200, 200, 200, 255) :
                           IM_COL32(150, 150, 150, 255);

    drawList->AddCircleFilled(pos, selected ? 6.0f : 5.0f, color);
    drawList->AddCircle(pos, selected ? 6.0f : 5.0f, IM_COL32(255, 255, 255, 255));
}

void CurveEditor::DrawBezierCurve(ImDrawList* drawList, const AnimationCurve& curve,
                                   const ImVec2& origin, const ImVec2& size,
                                   float minValue, float maxValue, ImU32 color) {
    const int segments = 64;

    auto valueToScreen = [&](float t, float v) -> ImVec2 {
        float normalizedV = (v - minValue) / (maxValue - minValue);
        return ImVec2(
            origin.x + t * size.x,
            origin.y + (1.0f - normalizedV) * size.y
        );
    };

    ImVec2 prevPoint = valueToScreen(0.0f, curve.Evaluate(0.0f));

    for (int i = 1; i <= segments; ++i) {
        float t = static_cast<float>(i) / segments;
        float value = curve.Evaluate(t);
        ImVec2 point = valueToScreen(t, value);
        drawList->AddLine(prevPoint, point, color, 2.0f);
        prevPoint = point;
    }
}

void CurveEditor::DrawGradientBar(ImDrawList* drawList, const Gradient& gradient,
                                   const ImVec2& pos, const ImVec2& size) {
    const int segments = 32;

    for (int i = 0; i < segments; ++i) {
        float t0 = static_cast<float>(i) / segments;
        float t1 = static_cast<float>(i + 1) / segments;

        Float4 c0 = gradient.Evaluate(t0);
        Float4 c1 = gradient.Evaluate(t1);

        ImU32 col0 = IM_COL32(
            static_cast<int>(c0.x * 255),
            static_cast<int>(c0.y * 255),
            static_cast<int>(c0.z * 255),
            static_cast<int>(c0.w * 255)
        );
        ImU32 col1 = IM_COL32(
            static_cast<int>(c1.x * 255),
            static_cast<int>(c1.y * 255),
            static_cast<int>(c1.z * 255),
            static_cast<int>(c1.w * 255)
        );

        float x0 = pos.x + t0 * size.x;
        float x1 = pos.x + t1 * size.x;

        drawList->AddRectFilledMultiColor(
            ImVec2(x0, pos.y),
            ImVec2(x1, pos.y + size.y),
            col0, col1, col1, col0
        );
    }

    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(80, 80, 80, 255));
}

// ParticleWidgets implementation

bool ParticleWidgets::RangeSlider(const char* label, float& minVal, float& maxVal,
                                  float rangeMin, float rangeMax) {
    bool changed = false;
    ImGui::PushID(label);

    float values[2] = { minVal, maxVal };
    if (ImGui::DragFloat2(label, values, 0.01f, rangeMin, rangeMax)) {
        minVal = std::min(values[0], values[1]);
        maxVal = std::max(values[0], values[1]);
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

bool ParticleWidgets::Vector3Input(const char* label, Float3& vec) {
    float v[3] = { vec.x, vec.y, vec.z };
    if (ImGui::DragFloat3(label, v, 0.01f)) {
        vec = { v[0], v[1], v[2] };
        return true;
    }
    return false;
}

bool ParticleWidgets::AngleSlider(const char* label, float& angleDegrees,
                                  float minAngle, float maxAngle) {
    return ImGui::SliderFloat(label, &angleDegrees, minAngle, maxAngle, "%.1f deg");
}

bool ParticleWidgets::BurstEditor(const char* label, std::vector<BurstConfig>& bursts) {
    bool changed = false;

    ImGui::PushID(label);
    ImGui::Text("%s", label);

    for (size_t i = 0; i < bursts.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));

        auto& burst = bursts[i];

        ImGui::Separator();
        ImGui::Text("バースト %zu", i);

        changed |= ImGui::DragFloat("時間###Time", &burst.time, 0.01f, 0.0f, 100.0f);
        changed |= ImGui::DragInt("数量###Count", &burst.count, 1, 1, 1000);
        changed |= ImGui::DragInt("繰り返し###Cycles", &burst.cycles, 1, 0, 100, "%d (0=Inf)");
        changed |= ImGui::DragFloat("間隔###Interval", &burst.interval, 0.01f, 0.0f, 10.0f);
        changed |= ImGui::SliderFloat("確率###Prob", &burst.probability, 0.0f, 1.0f);

        if (ImGui::Button("削除###Del")) {
            bursts.erase(bursts.begin() + i);
            changed = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (ImGui::Button("+ バースト追加###AddBurst")) {
        bursts.push_back(BurstConfig());
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

bool ParticleWidgets::ShapeCombo(const char* label, EmitShape& shape) {
    const char* names[] = {
        "点", "球", "半球", "ボックス", "コーン", "円", "エッジ"
    };

    int current = static_cast<int>(shape);
    if (ImGui::Combo(label, &current, names, IM_ARRAYSIZE(names))) {
        shape = static_cast<EmitShape>(current);
        return true;
    }
    return false;
}

bool ParticleWidgets::BlendModeCombo(const char* label, BlendMode& mode) {
    const char* names[] = {
        "加算", "アルファブレンド", "乗算", "プリマルチプライ"
    };

    int current = static_cast<int>(mode);
    if (ImGui::Combo(label, &current, names, IM_ARRAYSIZE(names))) {
        mode = static_cast<BlendMode>(current);
        return true;
    }
    return false;
}

bool ParticleWidgets::RenderModeCombo(const char* label, RenderMode& mode) {
    const char* names[] = {
        "ビルボード", "ストレッチビルボード", "水平ビルボード",
        "垂直ビルボード", "メッシュ", "トレイル"
    };

    int current = static_cast<int>(mode);
    if (ImGui::Combo(label, &current, names, IM_ARRAYSIZE(names))) {
        mode = static_cast<RenderMode>(current);
        return true;
    }
    return false;
}

} // namespace UnoEngine
