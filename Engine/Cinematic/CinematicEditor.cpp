#include "pch.h"
#include "CinematicEditor.h"
#include "../Scene/SceneSerializer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <filesystem>

#define U8(str) reinterpret_cast<const char*>(u8##str)

namespace UnoEngine {

// ============================================================
// 色定数
// ============================================================
namespace {
    constexpr ImU32 kColBackground  = IM_COL32( 28,  28,  28, 255);
    constexpr ImU32 kColRuler       = IM_COL32( 45,  45,  45, 255);
    constexpr ImU32 kColRulerText   = IM_COL32(180, 180, 180, 255);
    constexpr ImU32 kColTrack       = IM_COL32( 38,  38,  38, 255);
    constexpr ImU32 kColTrackBorder = IM_COL32( 60,  60,  60, 255);
    constexpr ImU32 kColScrub       = IM_COL32(255,  60,  60, 255);
    constexpr ImU32 kColKfNormal    = IM_COL32(255, 200,  50, 255);
    constexpr ImU32 kColKfSelected  = IM_COL32(255, 255, 100, 255);
    constexpr ImU32 kColKfHover     = IM_COL32(255, 230,  80, 255);
    constexpr ImU32 kColRecord      = IM_COL32(220,  40,  40, 255);
    constexpr ImU32 kColEventTrack  = IM_COL32( 35,  42,  50, 255);
    constexpr ImU32 kColEventBar    = IM_COL32( 80, 160, 220, 180);
    constexpr ImU32 kColEventBarSel = IM_COL32(100, 200, 255, 220);
    constexpr ImU32 kColEventText   = IM_COL32( 60, 180, 120, 180);
    constexpr ImU32 kColEventAudio  = IM_COL32(200, 140,  60, 180);
    constexpr ImU32 kColEventObj    = IM_COL32(180,  80, 180, 180);
    constexpr ImU32 kColEventLua    = IM_COL32(120, 120, 220, 180);
    constexpr ImU32 kColEventWait   = IM_COL32(220,  60,  60, 180);
    constexpr float kHeaderWidth    = 70.0f;
}

// ============================================================
// Update（毎フレーム）
// ============================================================
void CinematicEditor::Update(float deltaTime) {
    if (player_.IsPlaying()) {
        player_.Update(deltaTime);
        scrubTime_ = player_.GetCurrentTime();
    }
}

// ============================================================
// RenderWindow
// ============================================================
void CinematicEditor::RenderWindow() {
    if (!isOpen_) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::Begin(U8("シネマティック"), &isOpen_, flags)) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();
    RenderTimeline();
    ImGui::Separator();
    if (selectedEvent_ >= 0) {
        RenderEventInspector();
    } else {
        RenderKeyframeInspector();
    }

    // [F]キーでキーフレーム打刻（ウィンドウフォーカス中）
    if (recordMode_ && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if (ImGui::IsKeyPressed(ImGuiKey_F)) {
            StampKeyframeFromCamera();
        }
    }

    // [Delete]キーで選択キーフレーム削除
    if (selectedKeyframe_ >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelectedKeyframe();
    }

    ImGui::End();
}

// ============================================================
// RenderToolbar
// ============================================================
void CinematicEditor::RenderToolbar() {
    const float btnW = 32.0f;
    const float btnH = 22.0f;

    // ---- 再生コントロール ----
    bool isPlaying = player_.IsPlaying();

    // [◀] 先頭へ
    if (ImGui::Button("##toStart", ImVec2(btnW, btnH))) {
        player_.Stop();
        scrubTime_ = 0.0f;
        ApplyPreviewAt(0.0f);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("先頭へ (Stop)"));
    ImGui::SameLine(0, 2);

    // 先頭ボタンのアイコンを DrawList で描く（既に描画済みのボタン上に重ねる）
    {
        ImVec2 p = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cx = p.x + btnW * 0.5f, cy = p.y + btnH * 0.5f;
        dl->AddRectFilled(ImVec2(cx - 6, cy - 5), ImVec2(cx - 4, cy + 5), IM_COL32(200,200,200,255));
        dl->AddTriangleFilled(ImVec2(cx + 4, cy - 5), ImVec2(cx + 4, cy + 5), ImVec2(cx - 4, cy), IM_COL32(200,200,200,255));
    }

    // [▶ / ⏸]
    if (isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 1.f));
        if (ImGui::Button("##pause", ImVec2(btnW, btnH))) {
            player_.Pause();
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("一時停止"));
        // Pause アイコン
        {
            ImVec2 p = ImGui::GetItemRectMin();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float cx = p.x + btnW * 0.5f, cy = p.y + btnH * 0.5f;
            dl->AddRectFilled(ImVec2(cx - 5, cy - 5), ImVec2(cx - 2, cy + 5), IM_COL32(240,240,240,255));
            dl->AddRectFilled(ImVec2(cx + 2, cy - 5), ImVec2(cx + 5, cy + 5), IM_COL32(240,240,240,255));
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.f));
        if (ImGui::Button("##play", ImVec2(btnW, btnH))) {
            if (player_.IsFinished()) player_.Stop();
            player_.SetCamera(previewCamera_);
            player_.SetSequence(sequence_);
            player_.Play();
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("再生 (Preview)"));
        // Play アイコン
        {
            ImVec2 p = ImGui::GetItemRectMin();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float cx = p.x + btnW * 0.5f - 1, cy = p.y + btnH * 0.5f;
            dl->AddTriangleFilled(ImVec2(cx - 5, cy - 5), ImVec2(cx - 5, cy + 5), ImVec2(cx + 6, cy), IM_COL32(240,240,240,255));
        }
    }
    ImGui::SameLine(0, 2);

    // [⏹] 停止
    if (ImGui::Button("##stop", ImVec2(btnW, btnH))) {
        player_.Stop();
        scrubTime_ = 0.0f;
        ApplyPreviewAt(0.0f);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("停止"));
    {
        ImVec2 p = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float m = 5.0f;
        dl->AddRectFilled(ImVec2(p.x + btnW * 0.5f - m, p.y + btnH * 0.5f - m),
                          ImVec2(p.x + btnW * 0.5f + m, p.y + btnH * 0.5f + m),
                          IM_COL32(220,220,220,255));
    }

    ImGui::SameLine(0, 8);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 8);

    // ---- Record モード ----
    // NOTE: PopStyleColor はボタン描画前の状態で行うこと
    //       (Button()内でrecordMode_が変化してもPush/Popが一致するように)
    bool wasRecordMode = recordMode_;
    if (wasRecordMode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.f));
    }
    if (ImGui::Button(U8("● REC"), ImVec2(60, btnH))) {
        recordMode_ = !recordMode_;
        if (recordMode_) player_.Stop();
    }
    if (wasRecordMode) ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(recordMode_
            ? U8("レコード中 - [F]キーでキーフレーム打刻")
            : U8("レコードモード ON"));
    }

    ImGui::SameLine(0, 4);

    // [+KF] ボタン（手動追加）
    if (ImGui::Button(U8("+KF"), ImVec2(36, btnH))) {
        StampKeyframeFromCamera();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("現在のシーンカメラ位置にキーフレームを追加"));

    ImGui::SameLine(0, 8);

    // ---- デフォルトイージング ----
    {
        const char* easingDefNames[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", U8("強調") };
        int defIdx = static_cast<int>(defaultEasing_);
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("##defEasing", &defIdx, easingDefNames, 5)) {
            defaultEasing_ = static_cast<CameraKeyframe::Easing>(defIdx);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("新規キーフレームのデフォルト補間モード"));
        ImGui::SameLine(0, 2);
        if (ImGui::Button(U8("全適用"), ImVec2(0, btnH))) {
            for (auto& k : sequence_.keyframes) {
                k.easing = defaultEasing_;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("全キーフレームの補間モードを一括変更"));
    }

    ImGui::SameLine(0, 12);

    // ---- 時間表示 ----
    int totalSec  = (int)sequence_.duration;
    int curMin    = (int)(scrubTime_) / 60;
    int curSec    = (int)(scrubTime_) % 60;
    int curMs     = (int)(std::fmod(scrubTime_, 1.0f) * 100.0f);
    int totMin    = totalSec / 60;
    int totSec2   = totalSec % 60;
    ImGui::TextDisabled("%02d:%02d.%02d / %02d:%02d", curMin, curSec, curMs, totMin, totSec2);

    ImGui::SameLine(0, 12);
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::DragFloat(U8("尺(秒)"), &sequence_.duration, 0.5f, 1.0f, 3600.0f, "%.1f")) {
        sequence_.duration = std::max(sequence_.duration, 0.5f);
    }

    ImGui::SameLine(0, 8);
    ImGui::Checkbox("Loop", &sequence_.loop);

    ImGui::SameLine(0, 12);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 8);

    // ---- ズーム ----
    ImGui::TextUnformatted(U8("ズーム:"));
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("##zoom", &timelineZoom_, 0.1f, 8.0f, "x%.1f");

    ImGui::SameLine(0, 12);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 8);

    // ---- 現在のファイル表示 ----
    ImGui::TextDisabled("[%s]", sequenceNameBuf_);
    ImGui::SameLine(0, 4);
    ImGui::TextDisabled("%s", filePathBuf_);
    ImGui::SameLine(0, 8);

    // ---- 名前を付けて保存 ----
    if (ImGui::Button(U8("名前を付けて保存"), ImVec2(0, btnH))) {
        strncpy_s(saveAsNameBuf_, sequenceNameBuf_, sizeof(saveAsNameBuf_) - 1);
        strncpy_s(saveAsPathBuf_, filePathBuf_, sizeof(saveAsPathBuf_) - 1);
        saveAsMessage_.clear();
        showSaveAsPopup_ = true;
        ImGui::OpenPopup(U8("##SaveAsPopup"));
    }
    ImGui::SameLine(0, 2);

    // ---- 上書き保存 ----
    if (ImGui::Button(U8("上書き保存"), ImVec2(0, btnH))) {
        sequence_.name = sequenceNameBuf_;
        bool ok = sequence_.SaveToFile(filePathBuf_);
        if (ok) {
            std::string key = sequenceNameBuf_;
            if (!key.empty()) {
                SceneSerializer::s_cinematicPaths[key] = filePathBuf_;
            }
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("現在のパスに上書き保存"));
    ImGui::SameLine(0, 2);

    // ---- 読み込み ----
    if (ImGui::Button(U8("読込"), ImVec2(44, btnH))) {
        auto loaded = CinematicSequence::LoadFromFile(filePathBuf_);
        if (loaded.has_value()) {
            sequence_ = std::move(*loaded);
            strncpy_s(sequenceNameBuf_, sequence_.name.c_str(), sizeof(sequenceNameBuf_) - 1);
            selectedKeyframe_ = -1;
            scrubTime_ = 0.0f;
        }
    }

    // ---- 名前を付けて保存ポップアップ ----
    if (showSaveAsPopup_) {
        ImGui::OpenPopup(U8("##SaveAsPopup"));
        showSaveAsPopup_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
    if (ImGui::BeginPopup(U8("##SaveAsPopup"))) {
        ImGui::Text(U8("名前を付けて保存"));
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text(U8("シネマティック名:"));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##SaveAsName", saveAsNameBuf_, sizeof(saveAsNameBuf_));

        ImGui::Spacing();
        ImGui::Text(U8("保存先パス:"));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##SaveAsPath", saveAsPathBuf_, sizeof(saveAsPathBuf_));

        // 既存ファイル警告
        if (std::filesystem::exists(saveAsPathBuf_) &&
            std::string(saveAsPathBuf_) != std::string(filePathBuf_)) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                U8("  既存ファイルを上書きします"));
        }

        ImGui::Spacing();

        if (!saveAsMessage_.empty()) {
            bool isError = saveAsMessage_.find(U8("失敗")) != std::string::npos;
            ImVec4 color = isError ? ImVec4(1,0.3f,0.3f,1) : ImVec4(0.3f,1,0.3f,1);
            ImGui::TextColored(color, "%s", saveAsMessage_.c_str());
            ImGui::Spacing();
        }

        if (ImGui::Button(U8("保存"), ImVec2(120, 0))) {
            sequence_.name = saveAsNameBuf_;
            bool ok = sequence_.SaveToFile(saveAsPathBuf_);
            if (ok) {
                // 成功: バッファを更新
                strncpy_s(sequenceNameBuf_, saveAsNameBuf_, sizeof(sequenceNameBuf_) - 1);
                strncpy_s(filePathBuf_, saveAsPathBuf_, sizeof(filePathBuf_) - 1);
                std::string key = saveAsNameBuf_;
                if (!key.empty()) {
                    SceneSerializer::s_cinematicPaths[key] = saveAsPathBuf_;
                }
                saveAsMessage_.clear();
                ImGui::CloseCurrentPopup();
            } else {
                saveAsMessage_ = U8("保存失敗");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(U8("キャンセル"), ImVec2(120, 0))) {
            saveAsMessage_.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ============================================================
// TimeToX / XToTime（タイムライン座標変換）
// ============================================================
float CinematicEditor::TimeToX(float t) const {
    float pps = kBasePixelsPerSec * timelineZoom_;
    return tlOriginX_ + kHeaderWidth + (t - timelineScrollX_) * pps;
}

float CinematicEditor::XToTime(float x) const {
    float pps = kBasePixelsPerSec * timelineZoom_;
    return (x - tlOriginX_ - kHeaderWidth) / pps + timelineScrollX_;
}

// ============================================================
// RenderTimeline
// ============================================================
void CinematicEditor::RenderTimeline() {
    float pps = kBasePixelsPerSec * timelineZoom_;

    // 全体の高さ (カメラトラック + イベントトラック)
    float timelineHeight = kRulerHeight + kTrackHeight + kEventTrackHeight + 4.0f;

    // ---- スクロール可能な子ウィンドウ ----
    ImGui::BeginChild("##tl_scroll", ImVec2(-1, timelineHeight + 12.0f),
                      false, ImGuiWindowFlags_HorizontalScrollbar);

    // スクロールバー操作 → timelineScrollX_ に反映
    float scrollX   = ImGui::GetScrollX();
    timelineScrollX_ = scrollX / pps;

    // コンテンツ幅（シーケンス尺 + 여유）
    float contentWidth = kHeaderWidth + sequence_.duration * pps + 200.0f;
    ImGui::Dummy(ImVec2(contentWidth, timelineHeight));

    // ---- 描画リスト取得 ----
    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      winPos = ImGui::GetWindowPos();
    ImVec2      avail  = ImGui::GetWindowSize();

    // タイムライン左上（スクリーン座標）
    tlOriginX_ = winPos.x;
    tlOriginY_ = winPos.y;
    tlWidth_   = avail.x;

    // ============================================================
    // ルーラー背景
    // ============================================================
    ImVec2 rulerMin = { tlOriginX_, tlOriginY_ };
    ImVec2 rulerMax = { tlOriginX_ + tlWidth_, tlOriginY_ + kRulerHeight };
    dl->AddRectFilled(rulerMin, rulerMax, kColRuler);

    // トラック背景
    ImVec2 trackMin = { tlOriginX_, tlOriginY_ + kRulerHeight };
    ImVec2 trackMax = { tlOriginX_ + tlWidth_, tlOriginY_ + kRulerHeight + kTrackHeight };
    dl->AddRectFilled(trackMin, trackMax, kColTrack);
    dl->AddRect(trackMin, trackMax, kColTrackBorder);

    // ---- トラックヘッダー ----
    dl->AddRectFilled({ tlOriginX_, tlOriginY_ + kRulerHeight },
                      { tlOriginX_ + kHeaderWidth, tlOriginY_ + kRulerHeight + kTrackHeight },
                      IM_COL32(50, 50, 60, 255));
    dl->AddText({ tlOriginX_ + 6, tlOriginY_ + kRulerHeight + 8 },
                kColRulerText, U8("Camera"));

    // ---- 目盛り（秒単位）----
    // 適切な間隔を選択
    float minPixPerMark = 40.0f;
    float markInterval  = 1.0f;
    while (markInterval * pps < minPixPerMark) markInterval *= 2.0f;
    while (markInterval * pps > minPixPerMark * 4.0f && markInterval > 0.5f) markInterval *= 0.5f;

    float startT = std::floor(timelineScrollX_ / markInterval) * markInterval;
    float endT   = timelineScrollX_ + (tlWidth_ - kHeaderWidth) / pps + markInterval;

    for (float t = startT; t <= endT; t += markInterval) {
        if (t < 0.0f) continue;
        float x = TimeToX(t);
        if (x < tlOriginX_ + kHeaderWidth) continue;
        if (x > tlOriginX_ + tlWidth_)     break;

        // 縦線（ルーラー + トラック）
        dl->AddLine({ x, rulerMin.y + 12 }, { x, trackMax.y }, IM_COL32(65, 65, 65, 255));

        // 時間ラベル
        auto label = std::format("{:.1f}s", t);
        dl->AddText({ x + 2, tlOriginY_ + 4 }, kColRulerText, label.c_str());
    }

    // 0.1秒単位スナップ
    auto snap001 = [](float t) { return std::round(t / 0.1f) * 0.1f; };

    // ---- ルーラークリック → シーク (0.01秒スナップ) ----
    {
        ImVec2 rulerBtnMin = { tlOriginX_ + kHeaderWidth, tlOriginY_ };
        ImVec2 rulerBtnMax = { tlOriginX_ + tlWidth_, tlOriginY_ + kRulerHeight };
        ImGui::SetCursorScreenPos(rulerBtnMin);
        ImGui::InvisibleButton("##ruler", { rulerBtnMax.x - rulerBtnMin.x, kRulerHeight });

        if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float mx    = ImGui::GetIO().MousePos.x;
            float seekT = snap001(std::clamp(XToTime(mx), 0.0f, sequence_.duration));
            scrubTime_  = seekT;
            if (!player_.IsPlaying()) ApplyPreviewAt(seekT);
        }
    }

    // ---- スクラブヘッド（縦線） ----
    {
        float sx = TimeToX(scrubTime_);
        if (sx >= tlOriginX_ + kHeaderWidth && sx <= tlOriginX_ + tlWidth_) {
            dl->AddLine({ sx, tlOriginY_ }, { sx, tlOriginY_ + kRulerHeight + kTrackHeight },
                        kColScrub, 2.0f);
            // ヘッドのダイヤモンド（ルーラー下端）
            float hx = sx, hy = tlOriginY_ + kRulerHeight;
            ImVec2 pts[4] = {
                { hx,      hy - 7 },
                { hx + 6,  hy },
                { hx,      hy + 5 },
                { hx - 6,  hy }
            };
            dl->AddConvexPolyFilled(pts, 4, kColScrub);
        }

        // スクラブヘッドドラッグ (0.01秒スナップ)
        {
            float dragW = 14.0f;
            float sx2   = TimeToX(scrubTime_);
            ImGui::SetCursorScreenPos({ sx2 - dragW * 0.5f, tlOriginY_ });
            ImGui::InvisibleButton("##scrub_drag", { dragW, kRulerHeight });
            if (ImGui::IsItemActive()) {
                float mx    = ImGui::GetIO().MousePos.x;
                float seekT = snap001(std::clamp(XToTime(mx), 0.0f, sequence_.duration));
                scrubTime_  = seekT;
                if (!player_.IsPlaying()) ApplyPreviewAt(seekT);
            }
        }
    }

    // ---- イベントトラック背景 ----
    float eventTrackTop = tlOriginY_ + kRulerHeight + kTrackHeight;
    ImVec2 evTrackMin = { tlOriginX_, eventTrackTop };
    ImVec2 evTrackMax = { tlOriginX_ + tlWidth_, eventTrackTop + kEventTrackHeight };
    dl->AddRectFilled(evTrackMin, evTrackMax, kColEventTrack);
    dl->AddRect(evTrackMin, evTrackMax, kColTrackBorder);

    // イベントトラックヘッダー
    dl->AddRectFilled({ tlOriginX_, eventTrackTop },
                      { tlOriginX_ + kHeaderWidth, eventTrackTop + kEventTrackHeight },
                      IM_COL32(50, 55, 65, 255));
    dl->AddText({ tlOriginX_ + 6, eventTrackTop + 6 }, kColRulerText, U8("Events"));

    // ---- イベントバー描画 ----
    for (int i = 0; i < (int)sequence_.events.size(); ++i) {
        auto& ev = sequence_.events[i];
        float evStartX = TimeToX(ev.time);
        float evEndX   = TimeToX(ev.time + ev.duration);
        if (evEndX < tlOriginX_ + kHeaderWidth) continue;
        if (evStartX > tlOriginX_ + tlWidth_) continue;

        evStartX = std::max(evStartX, tlOriginX_ + kHeaderWidth);
        float barY = eventTrackTop + 3.0f;
        float barH = kEventTrackHeight - 6.0f;

        // タイプごとの色
        ImU32 barCol;
        switch (ev.type) {
            case CinematicEventType::Text:         barCol = kColEventText;  break;
            case CinematicEventType::Audio:        barCol = kColEventAudio; break;
            case CinematicEventType::ObjectToggle: barCol = kColEventObj;   break;
            case CinematicEventType::Lua:          barCol = kColEventLua;   break;
            case CinematicEventType::WaitForInput: barCol = kColEventWait;  break;
            default:                               barCol = kColEventBar;   break;
        }

        bool isSelected = (i == selectedEvent_);
        if (isSelected) barCol = kColEventBarSel;

        float minBarW = 6.0f;
        if (evEndX - evStartX < minBarW) evEndX = evStartX + minBarW;

        dl->AddRectFilled({ evStartX, barY }, { evEndX, barY + barH }, barCol, 3.0f);
        dl->AddRect({ evStartX, barY }, { evEndX, barY + barH }, IM_COL32(200,200,200,80), 3.0f);

        // ラベル
        const char* typeLabel = "";
        switch (ev.type) {
            case CinematicEventType::Text:         typeLabel = "T"; break;
            case CinematicEventType::Audio:        typeLabel = "A"; break;
            case CinematicEventType::ObjectToggle: typeLabel = "O"; break;
            case CinematicEventType::Lua:          typeLabel = "L"; break;
            case CinematicEventType::WaitForInput: typeLabel = "W"; break;
        }
        if (evEndX - evStartX > 12.0f) {
            dl->AddText({ evStartX + 2, barY + 2 }, IM_COL32(255,255,255,200), typeLabel);
        }

        // クリック判定
        ImGui::SetCursorScreenPos({ evStartX, barY });
        float btnW = std::max(evEndX - evStartX, minBarW);
        ImGui::InvisibleButton(std::format("##ev{}", i).c_str(), { btnW, barH });

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selectedEvent_ = i;
            selectedKeyframe_ = -1;
        }

        // ドラッグで時間移動
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float newT = snap001(std::clamp(XToTime(ImGui::GetIO().MousePos.x), 0.0f,
                                            sequence_.duration - ev.duration));
            ev.time = newT;
        }

        // 右クリックメニュー
        if (ImGui::BeginPopupContextItem(std::format("##ev_ctx{}", i).c_str())) {
            if (ImGui::MenuItem(U8("削除"))) {
                sequence_.events.erase(sequence_.events.begin() + i);
                selectedEvent_ = -1;
                ImGui::EndPopup();
                break;
            }
            ImGui::EndPopup();
        }
    }

    // ---- イベントトラック右クリック → 新規追加 ----
    {
        ImGui::SetCursorScreenPos({ tlOriginX_ + kHeaderWidth, eventTrackTop });
        ImGui::InvisibleButton("##evTrackBg", { tlWidth_ - kHeaderWidth, kEventTrackHeight });
        if (ImGui::BeginPopupContextItem("##evTrackAddMenu")) {
            float clickTime = snap001(std::clamp(XToTime(ImGui::GetIO().MousePos.x), 0.0f, sequence_.duration));

            auto addEvent = [&](CinematicEventType type) {
                CinematicEvent ev;
                ev.type = type;
                ev.time = clickTime;
                ev.duration = (type == CinematicEventType::WaitForInput) ? 0.0f : 3.0f;
                sequence_.events.push_back(ev);
                selectedEvent_ = static_cast<int>(sequence_.events.size()) - 1;
                selectedKeyframe_ = -1;
            };

            if (ImGui::MenuItem(U8("テキスト追加")))       addEvent(CinematicEventType::Text);
            if (ImGui::MenuItem(U8("オーディオ追加")))     addEvent(CinematicEventType::Audio);
            if (ImGui::MenuItem(U8("オブジェクト切替追加"))) addEvent(CinematicEventType::ObjectToggle);
            if (ImGui::MenuItem(U8("Lua追加")))           addEvent(CinematicEventType::Lua);
            if (ImGui::MenuItem(U8("入力待ち追加")))       addEvent(CinematicEventType::WaitForInput);
            ImGui::EndPopup();
        }
    }

    // スクラブ線をイベントトラックまで延長
    {
        float sx = TimeToX(scrubTime_);
        if (sx >= tlOriginX_ + kHeaderWidth && sx <= tlOriginX_ + tlWidth_) {
            dl->AddLine({ sx, eventTrackTop }, { sx, eventTrackTop + kEventTrackHeight },
                        kColScrub, 1.5f);
        }
    }

    // ---- キーフレームダイヤモンド ----
    float trackCY = tlOriginY_ + kRulerHeight + kTrackHeight * 0.5f;

    for (int i = 0; i < (int)sequence_.keyframes.size(); ++i) {
        auto& kf = sequence_.keyframes[i];
        float kx  = TimeToX(kf.time);
        if (kx < tlOriginX_ + kHeaderWidth - kDiamondRadius) continue;
        if (kx > tlOriginX_ + tlWidth_ + kDiamondRadius)     continue;

        bool isSelected = (i == selectedKeyframe_);
        ImVec2 pts[4] = {
            { kx,                 trackCY - kDiamondRadius },
            { kx + kDiamondRadius, trackCY },
            { kx,                 trackCY + kDiamondRadius },
            { kx - kDiamondRadius, trackCY }
        };

        // ホバー判定
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = mouse.x - kx, dy = mouse.y - trackCY;
        bool isHover = (std::abs(dx) + std::abs(dy)) < kDiamondRadius + 3.0f;

        ImU32 col = isSelected ? kColKfSelected : (isHover ? kColKfHover : kColKfNormal);
        dl->AddConvexPolyFilled(pts, 4, col);
        dl->AddPolyline(pts, 4, IM_COL32(80, 60, 0, 255), ImDrawFlags_Closed, 1.5f);

        // ---- キーフレームボタン ----
        // シングルクリック: 選択のみ
        // ダブルクリック + ホールド: 移動（0.01秒スナップ）
        float btnHalf = kDiamondRadius + 4.0f;
        ImGui::SetCursorScreenPos({ kx - btnHalf, trackCY - btnHalf });
        ImGui::InvisibleButton(std::format("##kf{}", i).c_str(), { btnHalf * 2, btnHalf * 2 });

        // シングルクリック → 選択のみ（ドラッグはしない）
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            selectedKeyframe_   = i;
            selectedEvent_      = -1;
            isDraggingKeyframe_ = false;
        }

        // ダブルクリック → 選択 + ドラッグ開始
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            selectedKeyframe_     = i;
            isDraggingKeyframe_   = true;
            dragKeyframeOrigTime_ = kf.time;
        }

        // ドラッグ中: マウスを押し続けている間だけ移動（0.01秒スナップ）
        if (isDraggingKeyframe_ && selectedKeyframe_ == i && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float newT = snap001(std::clamp(XToTime(ImGui::GetIO().MousePos.x), 0.0f, sequence_.duration));
            kf.time = newT;
        }

        // ドラッグ終了 → ソート＋再選択
        if (isDraggingKeyframe_ && selectedKeyframe_ == i && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            isDraggingKeyframe_ = false;
            float targetTime    = kf.time;
            SortKeyframes();
            for (int j = 0; j < (int)sequence_.keyframes.size(); ++j) {
                if (std::abs(sequence_.keyframes[j].time - targetTime) < 0.005f) {
                    selectedKeyframe_ = j;
                    break;
                }
            }
        }

        // 右クリックメニュー
        if (ImGui::BeginPopupContextItem(std::format("##kf_ctx{}", i).c_str())) {
            if (ImGui::MenuItem(U8("削除")))         { DeleteSelectedKeyframe(); }
            if (ImGui::MenuItem(U8("ここにシーク"))) { scrubTime_ = kf.time; ApplyPreviewAt(kf.time); }
            ImGui::EndPopup();
        }
    }

    // ---- マウスホイール → ズーム（Ctrl）or スクロール ----
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (ImGui::GetIO().KeyCtrl && wheel != 0.0f) {
            timelineZoom_ = std::clamp(timelineZoom_ + wheel * 0.15f, 0.1f, 10.0f);
        }
    }

    ImGui::EndChild();
}

// ============================================================
// RenderKeyframeInspector
// ============================================================
void CinematicEditor::RenderKeyframeInspector() {
    if (selectedKeyframe_ < 0 || selectedKeyframe_ >= (int)sequence_.keyframes.size()) {
        ImGui::TextDisabled(U8("キーフレームを選択するとここで編集できます"));
        return;
    }

    auto& kf = sequence_.keyframes[selectedKeyframe_];

    ImGui::Text(U8("キーフレーム #%d  (%.3f 秒)"), selectedKeyframe_, kf.time);
    ImGui::SameLine(0, 20);

    // 削除ボタン
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.f));
    if (ImGui::SmallButton(U8("削除"))) DeleteSelectedKeyframe();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    float colW = 120.0f;

    // ---- 時間 ----
    ImGui::SetNextItemWidth(colW);
    if (ImGui::DragFloat(U8("時間(秒)##kf_time"), &kf.time, 0.01f, 0.0f, sequence_.duration, "%.3f")) {
        SortKeyframes();
    }

    // ---- 位置 ----
    float pos[3] = { kf.position.GetX(), kf.position.GetY(), kf.position.GetZ() };
    ImGui::SetNextItemWidth(colW * 3 + 8.0f);
    if (ImGui::DragFloat3(U8("位置##kf_pos"), pos, 0.1f)) {
        kf.position = Vector3(pos[0], pos[1], pos[2]);
    }

    // ---- 回転（オイラー角表示） ----
    // Quaternion → Euler（YXZ順）
    auto& q = kf.rotation;
    float sinX = 2.0f * (q.GetW() * q.GetX() + q.GetY() * q.GetZ());
    float cosX = 1.0f - 2.0f * (q.GetX() * q.GetX() + q.GetY() * q.GetY());
    float sinY = 2.0f * (q.GetW() * q.GetY() - q.GetZ() * q.GetX());
    float sinZ = 2.0f * (q.GetW() * q.GetZ() + q.GetX() * q.GetY());
    float cosZ = 1.0f - 2.0f * (q.GetY() * q.GetY() + q.GetZ() * q.GetZ());

    constexpr float kRad2Deg = 180.0f / 3.14159265f;
    constexpr float kDeg2Rad = 3.14159265f / 180.0f;
    float euler[3] = {
        std::atan2(sinX, cosX) * kRad2Deg,
        std::asin(std::clamp(sinY, -1.0f, 1.0f)) * kRad2Deg,
        std::atan2(sinZ, cosZ) * kRad2Deg
    };

    ImGui::SetNextItemWidth(colW * 3 + 8.0f);
    if (ImGui::DragFloat3(U8("回転(度)##kf_rot"), euler, 0.5f)) {
        // Euler → Quaternion
        float ex = euler[0] * kDeg2Rad * 0.5f;
        float ey = euler[1] * kDeg2Rad * 0.5f;
        float ez = euler[2] * kDeg2Rad * 0.5f;
        kf.rotation = Quaternion(
            std::sin(ex) * std::cos(ey) * std::cos(ez) - std::cos(ex) * std::sin(ey) * std::sin(ez),
            std::cos(ex) * std::sin(ey) * std::cos(ez) + std::sin(ex) * std::cos(ey) * std::sin(ez),
            std::cos(ex) * std::cos(ey) * std::sin(ez) - std::sin(ex) * std::sin(ey) * std::cos(ez),
            std::cos(ex) * std::cos(ey) * std::cos(ez) + std::sin(ex) * std::sin(ey) * std::sin(ez)
        );
    }

    // ---- FOV ----
    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(colW);
    ImGui::DragFloat("FOV##kf_fov", &kf.fov, 0.5f, 5.0f, 170.0f, "%.1f°");

    // ---- イージング ----
    ImGui::SameLine(0, 20);
    const char* easingNames[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", U8("強調") };
    int easingIdx = static_cast<int>(kf.easing);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo(U8("補間##kf_ease"), &easingIdx, easingNames, 5)) {
        kf.easing = static_cast<CameraKeyframe::Easing>(easingIdx);
    }

    // ---- プレビューボタン ----
    ImGui::SameLine(0, 20);
    if (ImGui::SmallButton(U8("プレビュー"))) {
        scrubTime_ = kf.time;
        ApplyPreviewAt(kf.time);
    }

    // ---- 現在のシーンカメラで上書き ----
    ImGui::SameLine(0, 8);
    if (ImGui::SmallButton(U8("カメラから上書き"))) {
        if (sceneViewCamera_) {
            constexpr float kRad2Deg = 180.0f / 3.14159265f;
            kf.position = sceneViewCamera_->GetPosition();
            kf.rotation = sceneViewCamera_->GetRotation();
            kf.fov      = sceneViewCamera_->GetFieldOfView() * kRad2Deg;  // rad→deg
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(U8("Scene Viewカメラの現在ポーズでこのキーフレームを上書きします"));
    }
}

// ============================================================
// StampKeyframeFromCamera
// ============================================================
void CinematicEditor::StampKeyframeFromCamera() {
    if (!sceneViewCamera_) return;

    constexpr float kRad2Deg = 180.0f / 3.14159265f;
    CameraKeyframe kf;
    kf.time     = scrubTime_;
    kf.position = sceneViewCamera_->GetPosition();
    kf.rotation = sceneViewCamera_->GetRotation();
    kf.fov      = sceneViewCamera_->GetFieldOfView() * kRad2Deg;  // rad→deg
    kf.easing   = defaultEasing_;

    // 同じ時刻に既存のキーフレームがあれば上書き
    for (auto& existing : sequence_.keyframes) {
        if (std::abs(existing.time - kf.time) < 0.01f) {
            existing = kf;
            return;
        }
    }

    sequence_.keyframes.push_back(kf);
    SortKeyframes();
}

// ============================================================
// DeleteSelectedKeyframe
// ============================================================
void CinematicEditor::DeleteSelectedKeyframe() {
    if (selectedKeyframe_ < 0 || selectedKeyframe_ >= (int)sequence_.keyframes.size()) return;
    sequence_.keyframes.erase(sequence_.keyframes.begin() + selectedKeyframe_);
    selectedKeyframe_ = -1;
}

// ============================================================
// SortKeyframes
// ============================================================
void CinematicEditor::SortKeyframes() {
    std::sort(sequence_.keyframes.begin(), sequence_.keyframes.end(),
              [](const CameraKeyframe& a, const CameraKeyframe& b) {
                  return a.time < b.time;
              });
}

// ============================================================
// RenderEventInspector
// ============================================================
void CinematicEditor::RenderEventInspector() {
    if (selectedEvent_ < 0 || selectedEvent_ >= (int)sequence_.events.size()) {
        selectedEvent_ = -1;
        return;
    }

    auto& ev = sequence_.events[selectedEvent_];

    const char* typeNames[] = { U8("テキスト"), U8("オーディオ"), U8("オブジェクト切替"), "Lua", U8("入力待ち") };
    int typeIdx = static_cast<int>(ev.type);
    ImGui::Text(U8("イベント #%d  [%s]"), selectedEvent_, typeNames[typeIdx]);

    ImGui::SameLine(0, 20);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.f));
    if (ImGui::SmallButton(U8("削除##evDel"))) {
        sequence_.events.erase(sequence_.events.begin() + selectedEvent_);
        selectedEvent_ = -1;
        ImGui::PopStyleColor();
        return;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // タイプ変更
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::Combo(U8("タイプ##evType"), &typeIdx, typeNames, 5)) {
        ev.type = static_cast<CinematicEventType>(typeIdx);
    }

    ImGui::SameLine(0, 12);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat(U8("開始(秒)##evTime"), &ev.time, 0.01f, 0.0f, sequence_.duration, "%.3f");

    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat(U8("長さ(秒)##evDur"), &ev.duration, 0.01f, 0.0f, 60.0f, "%.2f");

    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(60.0f);
    ImGui::DragFloat(U8("FadeIn##evFI"), &ev.fadeIn, 0.01f, 0.0f, ev.duration, "%.2f");

    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(60.0f);
    ImGui::DragFloat(U8("FadeOut##evFO"), &ev.fadeOut, 0.01f, 0.0f, ev.duration, "%.2f");

    // タイプ固有パラメータ
    switch (ev.type) {
        case CinematicEventType::Text: {
            // テキスト内容（UTF-8、最大512バイト）
            static char textBuf[512] = {};
            if (selectedEvent_ >= 0) {
                strncpy_s(textBuf, ev.text.c_str(), sizeof(textBuf) - 1);
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText(U8("テキスト##evTxt"), textBuf, sizeof(textBuf))) {
                ev.text = textBuf;
            }

            const char* styleNames[] = { U8("字幕（下部）"), U8("中央ダイアログ"), U8("吹き出し") };
            int styleIdx = static_cast<int>(ev.displayStyle);
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::Combo(U8("表示スタイル##evStyle"), &styleIdx, styleNames, 3)) {
                ev.displayStyle = static_cast<TextDisplayStyle>(styleIdx);
            }

            ImGui::SameLine(0, 12);
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat(U8("フォントサイズ##evFS"), &ev.fontSize, 0.05f, 0.1f, 5.0f, "%.2f");
            break;
        }
        case CinematicEventType::Audio: {
            static char audioBuf[256] = {};
            strncpy_s(audioBuf, ev.audioClip.c_str(), sizeof(audioBuf) - 1);
            ImGui::SetNextItemWidth(300.0f);
            if (ImGui::InputText(U8("クリップ名##evAudio"), audioBuf, sizeof(audioBuf))) {
                ev.audioClip = audioBuf;
            }
            ImGui::SameLine(0, 8);
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat(U8("音量##evVol"), &ev.volume, 0.01f, 0.0f, 1.0f, "%.2f");
            break;
        }
        case CinematicEventType::ObjectToggle: {
            static char objBuf[256] = {};
            strncpy_s(objBuf, ev.targetObject.c_str(), sizeof(objBuf) - 1);
            ImGui::SetNextItemWidth(300.0f);
            if (ImGui::InputText(U8("オブジェクト名##evObj"), objBuf, sizeof(objBuf))) {
                ev.targetObject = objBuf;
            }
            ImGui::SameLine(0, 8);
            ImGui::Checkbox(U8("表示##evVis"), &ev.visible);
            break;
        }
        case CinematicEventType::Lua: {
            static char luaBuf[256] = {};
            strncpy_s(luaBuf, ev.luaFunction.c_str(), sizeof(luaBuf) - 1);
            ImGui::SetNextItemWidth(300.0f);
            if (ImGui::InputText(U8("Lua関数##evLua"), luaBuf, sizeof(luaBuf))) {
                ev.luaFunction = luaBuf;
            }
            break;
        }
        case CinematicEventType::WaitForInput: {
            static char keyBuf[64] = {};
            strncpy_s(keyBuf, ev.waitKey.c_str(), sizeof(keyBuf) - 1);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputText(U8("待ちキー##evKey"), keyBuf, sizeof(keyBuf))) {
                ev.waitKey = keyBuf;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(U8("空 = 任意キー"));
            break;
        }
    }
}

// ============================================================
// ApplyPreviewAt
// ============================================================
void CinematicEditor::ApplyPreviewAt(float time) {
    if (!previewCamera_) return;
    if (sequence_.keyframes.empty()) return;

    // CinematicPlayerを借りて評価
    CinematicPlayer tmp;
    tmp.SetSequence(sequence_);
    tmp.SetCamera(previewCamera_);
    tmp.Seek(time);
}

} // namespace UnoEngine
