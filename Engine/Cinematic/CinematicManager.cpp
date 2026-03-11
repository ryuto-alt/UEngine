#include "pch.h"
#include "CinematicManager.h"
#include "../Core/Logger.h"

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

} // namespace UnoEngine
