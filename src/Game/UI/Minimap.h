#pragma once
#include <memory>
#include <vector>
#include <string>
#include "../../Engine/Math/Mymath.h"
#include "MinimapGenerator.h"

class DirectXCommon;
class SrvManager;
class Camera;
class Sprite;
class SpriteCommon;
class BitmapFont;

class Minimap {
public:
    Minimap();
    ~Minimap();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
                    const std::string& mapTexturePath = "",
                    const std::string& boundsPath = "",
                    const std::string& framePath = "");
    void SetBitmapFont(BitmapFont* font) { bitmapFont_ = font; }
    void UpdateState(const Vector3& playerPos, float playerYaw,
                     const std::vector<Vector3>& uncollectedOrbPositions,
                     int totalOrbs, int collectedOrbs);
    void Draw();

    int GetTotalOrbs() const { return totalOrbs_; }
    int GetCollectedOrbs() const { return collectedOrbs_; }
    int GetRemainingOrbs() const { return totalOrbs_ - collectedOrbs_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // Minimap settings
    static constexpr float MAP_SIZE = 240.0f;
    static constexpr float MAP_MARGIN = 10.0f;
    static constexpr float MAP_SCALE = 3.0f;
    // Frame must cover rotated bg corners: MAP_SIZE * sqrt(2)/2 from center
    static constexpr float FRAME_SCALE = 1.45f;

    // NavMesh map bounds
    bool hasNavMeshMap_ = false;
    MinimapBounds bounds_{};
    int mapTexWidth_ = 512;
    int mapTexHeight_ = 512;

    // Sprites
    std::unique_ptr<SpriteCommon> spriteCommon_;
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> frameSprite_;      // Circular frame overlay
    std::unique_ptr<Sprite> playerSprite_;

    // Pre-allocated orb sprites (max 70)
    static constexpr int MAX_ORBS = 70;
    std::vector<std::unique_ptr<Sprite>> orbSprites_;
    std::vector<int> activeOrbIndices_;

    // Orb tracking
    int totalOrbs_ = 0;
    int collectedOrbs_ = 0;

    // BitmapFont for orb counter
    BitmapFont* bitmapFont_ = nullptr;

    // Cached position (bottom-right)
    float mapLeft_ = 0.0f;
    float mapTop_ = 0.0f;

    // Helper
    Vector2 WorldToMinimap(const Vector3& worldPos, const Vector3& playerPos, float yaw) const;
    bool IsInsideCircle(float screenX, float screenY) const;
};
