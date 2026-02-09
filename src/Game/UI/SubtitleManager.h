#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../../Engine/Math/Mymath.h"

class BitmapFont;
class SpriteCommon;
class Sprite;

class SubtitleManager {
public:
	struct Step {
		std::wstring text;
		float displayDuration = 5.0f;   // 全文表示後の待機時間(秒)
		float typeSpeed = 0.06f;         // 1文字あたりの表示間隔(秒)
	};

	SubtitleManager() = default;
	~SubtitleManager() = default;

	void Initialize(SpriteCommon* spriteCommon, BitmapFont* font);

	void SetSteps(std::vector<Step> steps);
	void Start();

	// deltaTime秒、skipキーが押されたか
	void Update(float deltaTime, bool skipPressed);
	void Draw();

	bool IsFinished() const { return m_finished; }
	bool IsActive() const { return m_active; }

	// フェードヒント（チュートリアル後に表示）
	void ShowHint(const std::wstring& text, float duration);
	bool IsHintActive() const { return m_hintActive; }

private:
	BitmapFont* m_font = nullptr;
	SpriteCommon* m_spriteCommon = nullptr;

	std::vector<Step> m_steps;
	int32_t m_currentStep = 0;
	bool m_active = false;
	bool m_finished = false;

	// タイプライター
	int32_t m_revealCount = 0;
	float m_typeTimer = 0.0f;
	bool m_fullyRevealed = false;

	// 全文表示後の待機
	float m_waitTimer = 0.0f;

	// 背景半透明帯
	std::unique_ptr<Sprite> m_bgSprite;

	// フェードヒント
	bool m_hintActive = false;
	std::wstring m_hintText;
	float m_hintDuration = 0.0f;
	float m_hintTimer = 0.0f;
	static constexpr float HINT_FADE_TIME = 0.8f;
};
