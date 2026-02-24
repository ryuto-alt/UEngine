#pragma once
#include "ECS/System.h"
#include <functional>
#include <vector>

namespace ECS {

// Main render pass: PostProcess chain, skybox, objects, player, orbs, enemy
class RenderSystem final : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    const char* GetName() const override { return "RenderSystem"; }

    // PostProcess適用前に追加描画するコールバックを登録
    void AddExtraDrawCallback(std::function<void()> callback) {
        extraDrawCallbacks_.push_back(std::move(callback));
    }
    void ClearExtraDrawCallbacks() { extraDrawCallbacks_.clear(); }

private:
    std::vector<std::function<void()>> extraDrawCallbacks_;
};

} // namespace ECS
