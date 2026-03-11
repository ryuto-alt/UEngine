#pragma once

#include "CinematicPlayer.h"
#include "CinematicSequence.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace UnoEngine {

class CinematicManager {
public:
    using FinishedCallback = std::function<void(std::string_view)>;

    void Register(std::string name, CinematicSequence sequence);
    bool RegisterFromFile(std::string name, const std::string& filepath);
    void Unregister(const std::string& name);

    void SetCamera(Camera* camera);

    bool Play(const std::string& name);
    void Pause();
    void Stop();

    bool IsPlaying() const;
    bool IsFinished() const;
    std::string_view GetCurrentName() const;

    void SetOnFinished(FinishedCallback cb);

    void Update(float deltaTime);

    // ImGuiでテキストイベントオーバーレイを描画
    // ImGui描画コンテキスト内で呼ぶこと
    void RenderTextOverlay(float viewWidth, float viewHeight) const;

    CinematicPlayer& GetPlayer() { return player_; }

private:
    std::unordered_map<std::string, CinematicSequence> sequences_;
    CinematicPlayer  player_;
    Camera*          camera_ = nullptr;
    std::string      currentName_;
    FinishedCallback onFinished_;
    bool             wasPlaying_ = false;
};

} // namespace UnoEngine
