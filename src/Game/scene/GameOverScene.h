#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "PostProcess.h"
#include "../UI/SettingsMenu.h"
#include <memory>

class GameOverScene : public IScene {
public:
    GameOverScene() = default;
    ~GameOverScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    // 背景レイヤー
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> bloodOverlaySprite_;

    // テキストレイヤー
    std::unique_ptr<Sprite> gameOverTextSprite_;

    // メニューボタン
    std::unique_ptr<Sprite> continueSprite_;
    std::unique_ptr<Sprite> quitSprite_;
    std::unique_ptr<Sprite> selectorSprite_;

    // 色収差スプライト（GAME OVER用）
    std::unique_ptr<Sprite> gameOverRedSprite_;
    std::unique_ptr<Sprite> gameOverBlueSprite_;

    // 色収差スプライト（Continue用）
    std::unique_ptr<Sprite> continueRedSprite_;
    std::unique_ptr<Sprite> continueBlueSprite_;

    // 色収差スプライト（Quit用）
    std::unique_ptr<Sprite> quitRedSprite_;
    std::unique_ptr<Sprite> quitBlueSprite_;

    // ポストプロセスエフェクト
    std::unique_ptr<PostProcess> horrorEffect_;
    std::unique_ptr<PostProcess> vhsEffect_;
    float time_ = 0.0f;

    // 色収差アニメーション
    float chromaticTimer_ = 0.0f;
    float glitchCooldown_ = 0.0f;
    float glitchDuration_ = 0.0f;
    float glitchOffsetX_ = 0.0f;
    float glitchOffsetY_ = 0.0f;

    // フェードイン演出
    float fadeInTimer_ = 0.0f;
    float fadeInDuration_ = 2.0f;

    // メニュー選択
    enum class MenuSelection {
        Continue = 0,
        Quit = 1
    };
    MenuSelection currentSelection_ = MenuSelection::Continue;
    bool CheckMouseHover(const Vector2& mousePos, const Vector2& spritePos, const Vector2& spriteSize);

    // 選択エフェクト用
    Vector2 continueOriginalSize_;
    Vector2 quitOriginalSize_;

    // パルスアニメーション
    float pulseTimer_ = 0.0f;

    // 設定メニュー
    std::unique_ptr<SettingsMenu> settingsMenu_;
};
