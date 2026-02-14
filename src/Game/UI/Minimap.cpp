#include "Minimap.h"
#include "BitmapFont.h"
#include "../../Engine/Graphics/DirectXCommon.h"
#include "../../Engine/Graphics/SRVManager.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Graphics/TextureManager.h"
#include "../../Engine/Utility/WinApp.h"
#include <cmath>
#include <filesystem>
#include <sstream>

Minimap::Minimap() = default;
Minimap::~Minimap() = default;

void Minimap::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
                         const std::string& mapTexturePath,
                         const std::string& boundsPath,
                         const std::string& framePath) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // Bottom-right position
    float windowW = static_cast<float>(WinApp::kClientWidth);
    float windowH = static_cast<float>(WinApp::kClientHeight);
    mapLeft_ = windowW - MAP_SIZE - MAP_MARGIN;
    mapTop_ = windowH - MAP_SIZE - MAP_MARGIN;

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_);

    const std::string whiteTex = "Resources/textures/white1x1.png";

    // Check if NavMesh map texture exists
    hasNavMeshMap_ = !mapTexturePath.empty() && std::filesystem::exists(mapTexturePath);
    if (hasNavMeshMap_ && !boundsPath.empty()) {
        bounds_ = MinimapGenerator::LoadBounds(boundsPath);
    }

    // Background - use NavMesh texture if available
    // Anchor at center for rotation; circular frame masks corners so no enlargement needed
    float bgCenterX = mapLeft_ + MAP_SIZE * 0.5f;
    float bgCenterY = mapTop_ + MAP_SIZE * 0.5f;

    backgroundSprite_ = std::make_unique<Sprite>();
    if (hasNavMeshMap_) {
        backgroundSprite_->Initialize(spriteCommon_.get(), mapTexturePath);
        backgroundSprite_->SetPosition({ bgCenterX, bgCenterY });
        backgroundSprite_->SetSize({ MAP_SIZE, MAP_SIZE });
        backgroundSprite_->SetAnchorPoint({ 0.5f, 0.5f });
        backgroundSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        backgroundSprite_->SetTextureLeftTop({ 0.0f, 0.0f });
        backgroundSprite_->SetTextureSize({ static_cast<float>(mapTexWidth_), static_cast<float>(mapTexHeight_) });
    } else {
        backgroundSprite_->Initialize(spriteCommon_.get(), whiteTex);
        backgroundSprite_->SetPosition({ bgCenterX, bgCenterY });
        backgroundSprite_->SetSize({ MAP_SIZE, MAP_SIZE });
        backgroundSprite_->SetAnchorPoint({ 0.5f, 0.5f });
        backgroundSprite_->setColor({ 0.02f, 0.01f, 0.05f, 0.85f });
    }

    // Circular frame overlay (drawn on top to mask rotated bg corners, NOT rotated)
    // Frame is larger than MAP_SIZE to cover the rotated background's protruding corners
    bool hasFrame = !framePath.empty() && std::filesystem::exists(framePath);
    if (hasFrame) {
        float frameSize = MAP_SIZE * FRAME_SCALE;
        frameSprite_ = std::make_unique<Sprite>();
        frameSprite_->Initialize(spriteCommon_.get(), framePath);
        frameSprite_->SetPosition({ bgCenterX, bgCenterY });
        frameSprite_->SetSize({ frameSize, frameSize });
        frameSprite_->SetAnchorPoint({ 0.5f, 0.5f });
        frameSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // Player icon - always points up (map rotates instead)
    playerSprite_ = std::make_unique<Sprite>();
    playerSprite_->Initialize(spriteCommon_.get(), "Resources/textures/UI/minimap_player.png");
    playerSprite_->SetSize({ 16.0f, 16.0f });
    playerSprite_->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // Pre-allocate orb sprites
    orbSprites_.reserve(MAX_ORBS);
    for (int i = 0; i < MAX_ORBS; ++i) {
        auto s = std::make_unique<Sprite>();
        s->Initialize(spriteCommon_.get(), "Resources/textures/UI/minimap_orb.png");
        s->SetSize({ 10.0f, 10.0f });
        s->setColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        orbSprites_.push_back(std::move(s));
    }
}

void Minimap::UpdateState(const Vector3& playerPos, float playerYaw,
                          const std::vector<Vector3>& uncollectedOrbPositions,
                          int totalOrbs, int collectedOrbs) {
    totalOrbs_ = totalOrbs;
    collectedOrbs_ = collectedOrbs;

    // Player icon at minimap center (always points up)
    float centerX = mapLeft_ + MAP_SIZE * 0.5f - 8.0f;
    float centerY = mapTop_ + MAP_SIZE * 0.5f - 8.0f;
    if (playerSprite_) {
        playerSprite_->SetPosition({centerX, centerY});
    }

    // Rotate NavMesh map background to match player facing direction
    if (backgroundSprite_) {
        backgroundSprite_->SetRotation(-playerYaw);

        if (hasNavMeshMap_) {
            float boundsWidth = bounds_.maxX - bounds_.minX;
            float boundsHeight = bounds_.maxZ - bounds_.minZ;
            if (boundsWidth > 0.0f && boundsHeight > 0.0f) {
                float u = (playerPos.x - bounds_.minX) / boundsWidth * mapTexWidth_;
                float v = (bounds_.maxZ - playerPos.z) / boundsHeight * mapTexHeight_;

                float worldVisible = MAP_SIZE / MAP_SCALE;
                float uvSizeX = (worldVisible / boundsWidth) * mapTexWidth_;
                float uvSizeY = (worldVisible / boundsHeight) * mapTexHeight_;

                backgroundSprite_->SetTextureLeftTop({
                    u - uvSizeX * 0.5f,
                    v - uvSizeY * 0.5f
                });
                backgroundSprite_->SetTextureSize({ uvSizeX, uvSizeY });
            }
        }
    }

    // Update orb sprites - rotated by player yaw, circular clipping
    activeOrbIndices_.clear();
    float halfMap = MAP_SIZE * 0.5f;

    for (int i = 0; i < static_cast<int>(uncollectedOrbPositions.size()) && i < MAX_ORBS; ++i) {
        Vector2 offset = WorldToMinimap(uncollectedOrbPositions[i], playerPos, playerYaw);

        float sx = mapLeft_ + halfMap + offset.x - 5.0f;
        float sy = mapTop_ + halfMap + offset.y - 5.0f;

        if (!IsInsideCircle(sx + 5.0f, sy + 5.0f)) continue;

        orbSprites_[i]->SetPosition({sx, sy});
        activeOrbIndices_.push_back(i);
    }
}

void Minimap::Draw() {
    if (!spriteCommon_) return;

    spriteCommon_->CommonDraw();

    // Layer order: background (rotated) -> orbs -> player -> frame overlay
    if (backgroundSprite_) {
        backgroundSprite_->Update();
        backgroundSprite_->Draw();
    }

    for (int idx : activeOrbIndices_) {
        if (idx < static_cast<int>(orbSprites_.size()) && orbSprites_[idx]) {
            orbSprites_[idx]->Update();
            orbSprites_[idx]->Draw();
        }
    }

    if (playerSprite_) {
        playerSprite_->Update();
        playerSprite_->Draw();
    }

    // Frame overlay on top (masks corners, NOT rotated)
    if (frameSprite_) {
        frameSprite_->Update();
        frameSprite_->Draw();
    }

    // Orb counter above minimap
    int remaining = totalOrbs_ - collectedOrbs_;
    if (bitmapFont_) {
        float pct = totalOrbs_ > 0 ? static_cast<float>(collectedOrbs_) / totalOrbs_ : 0.0f;
        Vector4 color;
        if (pct >= 1.0f)      color = {0.2f, 1.0f, 0.4f, 1.0f};
        else if (pct >= 0.5f) color = {1.0f, 0.9f, 0.2f, 1.0f};
        else                  color = {1.0f, 0.6f, 0.1f, 1.0f};

        std::wstringstream wss;
        wss << L"ORBS: " << remaining << L" / " << totalOrbs_;
        std::wstring text = wss.str();

        float fontScale = 1.0f;
        float textWidth = bitmapFont_->MeasureTextWidth(text, fontScale);
        float textX = mapLeft_ + (MAP_SIZE - textWidth) * 0.5f;
        float textY = mapTop_ - 28.0f;

        bitmapFont_->BeginDraw();
        bitmapFont_->RenderText(text, {textX, textY}, fontScale, color);
    }
}

Vector2 Minimap::WorldToMinimap(const Vector3& worldPos, const Vector3& playerPos, float yaw) const {
    float dx = worldPos.x - playerPos.x;
    float dz = worldPos.z - playerPos.z;

    // Convert to screen space (before rotation)
    float sx = dx * MAP_SCALE;
    float sy = -dz * MAP_SCALE;

    // Rotate by -playerYaw so player's forward is always up
    float cosA = std::cos(-yaw);
    float sinA = std::sin(-yaw);
    float rx = sx * cosA - sy * sinA;
    float ry = sx * sinA + sy * cosA;

    return { rx, ry };
}

bool Minimap::IsInsideCircle(float screenX, float screenY) const {
    float cx = mapLeft_ + MAP_SIZE * 0.5f;
    float cy = mapTop_ + MAP_SIZE * 0.5f;
    float radius = MAP_SIZE * 0.5f - 6.0f;
    float dx = screenX - cx;
    float dy = screenY - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}
