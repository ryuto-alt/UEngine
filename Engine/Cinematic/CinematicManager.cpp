#include "pch.h"
#include "CinematicManager.h"
#include "../Core/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace UnoEngine {

void CinematicManager::Register(std::string name, CinematicSequence sequence) {
    sequences_[std::move(name)] = std::move(sequence);
}

bool CinematicManager::RegisterFromFile(std::string name, const std::string& filepath) {
    if (sequences_.contains(name)) return true;

    auto seq = CinematicSequence::LoadFromFile(filepath);
    if (!seq.has_value()) {
        Logger::Warning("[CinematicManager] Failed to load: {}", filepath);
        return false;
    }
    sequences_[std::move(name)] = std::move(seq.value());
    return true;
}

void CinematicManager::Unregister(const std::string& name) {
    sequences_.erase(name);
}

void CinematicManager::SetCamera(Camera* camera) {
    camera_ = camera;
    player_.SetCamera(camera);
}

bool CinematicManager::Play(const std::string& name) {
    auto it = sequences_.find(name);
    if (it == sequences_.end()) {
        Logger::Warning("[CinematicManager] Sequence not found: {}", name);
        return false;
    }

    currentName_ = name;
    player_.SetSequence(it->second);
    if (camera_) player_.SetCamera(camera_);
    player_.Play();
    wasPlaying_ = true;
    Logger::Info("[CinematicManager] Playing: {}", name);
    return true;
}

void CinematicManager::Pause() {
    player_.Pause();
}

void CinematicManager::Stop() {
    player_.Stop();
    wasPlaying_ = false;
}

bool CinematicManager::IsPlaying() const {
    return player_.IsPlaying();
}

bool CinematicManager::IsFinished() const {
    return player_.IsFinished();
}

std::string_view CinematicManager::GetCurrentName() const {
    return currentName_;
}

void CinematicManager::SetOnFinished(FinishedCallback cb) {
    onFinished_ = std::move(cb);
}

void CinematicManager::Update(float deltaTime) {
    if (player_.IsPlaying()) {
        player_.Update(deltaTime);
    }

    // Finished edge detection: was playing last frame, now finished
    if (wasPlaying_ && player_.IsFinished()) {
        wasPlaying_ = false;
        if (onFinished_) {
            auto cb = std::move(onFinished_);
            onFinished_ = nullptr;
            cb(currentName_);
        }
    }
}

void CinematicManager::RenderTextOverlay(float viewWidth, float viewHeight) const {
    if (!player_.IsPlaying() && !player_.IsWaitingForInput()) return;

    float currentTime = player_.GetCurrentTime();
    auto activeEvents = player_.GetActiveEvents(currentTime);

    for (const auto* ev : activeEvents) {
        if (ev->type != CinematicEventType::Text) continue;
        if (ev->text.empty()) continue;

        float alpha = CinematicPlayer::ComputeEventAlpha(*ev, currentTime);
        if (alpha <= 0.001f) continue;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                  ImGuiWindowFlags_NoInputs |
                                  ImGuiWindowFlags_NoNav |
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoFocusOnAppearing;

        float fontSize = ev->fontSize;
        float padding = 12.0f;

        // テキストサイズ計算
        ImVec2 textSize = ImGui::CalcTextSize(ev->text.c_str());
        textSize.x *= fontSize;
        textSize.y *= fontSize;

        float winW = textSize.x + padding * 2;
        float winH = textSize.y + padding * 2;

        // 表示位置
        ImVec2 pos;
        switch (ev->displayStyle) {
            case TextDisplayStyle::Subtitle:
                pos = { (viewWidth - winW) * 0.5f, viewHeight - winH - 40.0f };
                break;
            case TextDisplayStyle::CenterDialog:
                pos = { (viewWidth - winW) * 0.5f, (viewHeight - winH) * 0.5f };
                break;
            case TextDisplayStyle::Bubble:
                pos = { (viewWidth - winW) * 0.5f, viewHeight * 0.3f };
                break;
        }

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.7f * alpha);

        auto windowId = std::format("##cinematic_text_{}", reinterpret_cast<uintptr_t>(ev));
        ImGui::Begin(windowId.c_str(), nullptr, flags);

        if (fontSize != 1.0f) {
            ImGui::SetWindowFontScale(fontSize);
        }

        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", ev->text.c_str());

        if (fontSize != 1.0f) {
            ImGui::SetWindowFontScale(1.0f);
        }

        ImGui::End();
    }

    // WaitForInput表示
    if (player_.IsWaitingForInput()) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                  ImGuiWindowFlags_NoInputs |
                                  ImGuiWindowFlags_NoNav |
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoFocusOnAppearing;

        float blinkAlpha = (std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f) + 1.0f) * 0.5f;

        ImVec2 pos = { viewWidth * 0.5f - 80.0f, viewHeight - 30.0f };
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##wait_input_hint", nullptr, flags);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, blinkAlpha * 0.8f), "Press any key...");
        ImGui::End();
    }
}

} // namespace UnoEngine
