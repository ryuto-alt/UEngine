#include "Minimap.h"
#include "../../Engine/Graphics/DirectXCommon.h"
#include "../../Engine/Graphics/SRVManager.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Graphics/TextureManager.h"
#include "../GameObject/Player.h"
#include "../GameObject/Orb.h"
#include "../../Engine/Utility/WinApp.h"
#include "imgui.h"
#include <cmath>

Minimap::Minimap() = default;
Minimap::~Minimap() = default;

void Minimap::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // Bottom-right position
    float windowW = static_cast<float>(WinApp::kClientWidth);
    float windowH = static_cast<float>(WinApp::kClientHeight);
    mapLeft_ = windowW - MAP_SIZE - MAP_MARGIN;
    mapTop_ = windowH - MAP_SIZE - MAP_MARGIN;

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_);

    const std::string whiteTex = "Resources/UI/white.png";

    // Border
    borderSprite_ = std::make_unique<Sprite>();
    borderSprite_->Initialize(spriteCommon_.get(), whiteTex);
    borderSprite_->SetPosition({ mapLeft_ - 2, mapTop_ - 2 });
    borderSprite_->SetSize({ MAP_SIZE + 4, MAP_SIZE + 4 });
    borderSprite_->setColor({ 0.5f, 0.5f, 0.5f, 0.9f });

    // Background
    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(spriteCommon_.get(), whiteTex);
    backgroundSprite_->SetPosition({ mapLeft_, mapTop_ });
    backgroundSprite_->SetSize({ MAP_SIZE, MAP_SIZE });
    backgroundSprite_->setColor({ 0.08f, 0.08f, 0.12f, 0.75f });

    // Player dot (always center)
    playerSprite_ = std::make_unique<Sprite>();
    playerSprite_->Initialize(spriteCommon_.get(), whiteTex);
    playerSprite_->SetSize({ 8.0f, 8.0f });
    playerSprite_->setColor({ 0.0f, 1.0f, 0.0f, 1.0f });

    // Pre-allocate orb sprites
    orbSprites_.reserve(MAX_ORBS);
    for (int i = 0; i < MAX_ORBS; ++i) {
        auto s = std::make_unique<Sprite>();
        s->Initialize(spriteCommon_.get(), whiteTex);
        s->SetSize({ 5.0f, 5.0f });
        s->setColor({ 1.0f, 0.85f, 0.0f, 1.0f });
        orbSprites_.push_back(std::move(s));
    }
}

void Minimap::Update(Player* player, const std::vector<std::unique_ptr<Orb>>& orbs) {
    if (!player) return;

    Vector3 playerWorldPos = player->GetPosition();
    float centerX = mapLeft_ + MAP_SIZE * 0.5f;
    float centerY = mapTop_ + MAP_SIZE * 0.5f;

    // Player is always at center
    playerSprite_->SetPosition({ centerX - 4.0f, centerY - 4.0f });

    // Update orbs
    activeOrbIndices_.clear();
    totalOrbs_ = 0;
    collectedOrbs_ = 0;
    int spriteIdx = 0;

    for (const auto& orb : orbs) {
        if (!orb) continue;
        totalOrbs_++;

        if (orb->IsCollected()) {
            collectedOrbs_++;
            continue;
        }

        if (spriteIdx >= MAX_ORBS) continue;

        Vector2 mapPos = WorldToMinimap(orb->GetPosition(), playerWorldPos);
        float sx = centerX + mapPos.x - 2.5f;
        float sy = centerY + mapPos.y - 2.5f;

        // Skip orbs outside minimap bounds
        float halfMap = MAP_SIZE * 0.5f - 4.0f;
        if (std::abs(sx - centerX) > halfMap || std::abs(sy - centerY) > halfMap) continue;

        orbSprites_[spriteIdx]->SetPosition({ sx, sy });
        activeOrbIndices_.push_back(spriteIdx);
        spriteIdx++;
    }
}

void Minimap::Draw() {
    if (!spriteCommon_) return;

    spriteCommon_->CommonDraw();

    // Back to front: border → background → orbs → player
    if (borderSprite_) {
        borderSprite_->Update();
        borderSprite_->Draw();
    }
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

    // Orb counter (ImGui overlay above minimap)
    int remaining = totalOrbs_ - collectedOrbs_;
    ImGui::SetNextWindowPos(ImVec2(mapLeft_, mapTop_ - 30.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(MAP_SIZE, 28.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::Begin("##OrbCounter", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs);

    // Color based on progress
    float pct = totalOrbs_ > 0 ? static_cast<float>(collectedOrbs_) / totalOrbs_ : 0.0f;
    ImVec4 color;
    if (pct >= 1.0f)      color = ImVec4(0.0f, 1.0f, 0.3f, 1.0f);  // Green
    else if (pct >= 0.5f) color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
    else                  color = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);  // Orange

    ImGui::TextColored(color, "ORBS: %d / %d", remaining, totalOrbs_);
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

Vector2 Minimap::WorldToMinimap(const Vector3& worldPos, const Vector3& playerPos) const {
    // Player-centered: offset from player position
    float dx = worldPos.x - playerPos.x;
    float dz = worldPos.z - playerPos.z;

    return { dx * MAP_SCALE, -dz * MAP_SCALE };
}
