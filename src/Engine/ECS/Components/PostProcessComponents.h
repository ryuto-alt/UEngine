#pragma once
#include <memory>
#include "PostProcess.h"
#include "UI/Minimap.h"
#include "UI/BitmapFont.h"
#include "UI/SubtitleManager.h"
#include "Sprite.h"

namespace ECS {

struct PostProcessChainComponent {
    std::unique_ptr<PostProcess> psxEffect;
    std::unique_ptr<PostProcess> horrorEffect;
    float fisheyeStrength = 2.58f;
    float fisheyeRadius = 1.5f;
};

struct MinimapComponent {
    std::unique_ptr<Minimap> minimap;
};

struct SubtitleUIComponent {
    std::unique_ptr<BitmapFont> bitmapFont;
    std::unique_ptr<SubtitleManager> subtitleManager;
};

struct FadeSpriteComponent {
    std::unique_ptr<Sprite> sprite;
};

struct SceneObjectTag {};

} // namespace ECS
