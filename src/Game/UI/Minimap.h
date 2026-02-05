#pragma once
#include <memory>
#include <vector>
#include <string>
#include "../../Engine/Math/Mymath.h"

class DirectXCommon;
class SrvManager;
class Camera;
class Player;
class Orb;
class Sprite;
class SpriteCommon;

class Minimap {
public:
    Minimap();
    ~Minimap();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(Player* player, const std::vector<std::unique_ptr<Orb>>& orbs);
    void Draw();

    int GetTotalOrbs() const { return totalOrbs_; }
    int GetCollectedOrbs() const { return collectedOrbs_; }
    int GetRemainingOrbs() const { return totalOrbs_ - collectedOrbs_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // Minimap settings
    static constexpr float MAP_SIZE = 180.0f;
    static constexpr float MAP_MARGIN = 10.0f;
    static constexpr float MAP_SCALE = 2.5f;  // pixels per world unit

    // Sprites
    std::unique_ptr<SpriteCommon> spriteCommon_;
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> borderSprite_;
    std::unique_ptr<Sprite> playerSprite_;

    // Pre-allocated orb sprites (max 70)
    static constexpr int MAX_ORBS = 70;
    std::vector<std::unique_ptr<Sprite>> orbSprites_;
    std::vector<int> activeOrbIndices_;

    // Orb tracking
    int totalOrbs_ = 0;
    int collectedOrbs_ = 0;

    // Cached position (bottom-right)
    float mapLeft_ = 0.0f;
    float mapTop_ = 0.0f;

    // Helper
    Vector2 WorldToMinimap(const Vector3& worldPos, const Vector3& playerPos) const;
};
