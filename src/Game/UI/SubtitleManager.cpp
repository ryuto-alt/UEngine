#include "SubtitleManager.h"
#include "BitmapFont.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Utility/WinApp.h"
#include <algorithm>

void SubtitleManager::Initialize(SpriteCommon* spriteCommon, BitmapFont* font) {
	m_spriteCommon = spriteCommon;
	m_font = font;

	// 字幕背景の半透明帯
	m_bgSprite = std::make_unique<Sprite>();
	m_bgSprite->Initialize(m_spriteCommon, "Resources/textures/common/white1x1.png");
}

void SubtitleManager::SetSteps(std::vector<Step> steps) {
	m_steps = std::move(steps);
}

void SubtitleManager::Start() {
	if (m_steps.empty()) return;

	m_active = true;
	m_finished = false;
	m_currentStep = 0;
	m_revealCount = 0;
	m_typeTimer = 0.0f;
	m_fullyRevealed = false;
	m_waitTimer = 0.0f;
	m_hintActive = false;
}

void SubtitleManager::Update(float deltaTime, bool skipPressed) {
	// フェードヒントの更新
	if (m_hintActive) {
		m_hintTimer += deltaTime;
		if (m_hintTimer >= m_hintDuration) {
			m_hintActive = false;
		}
	}

	if (!m_active || m_finished) return;
	if (m_currentStep >= static_cast<int32_t>(m_steps.size())) {
		m_finished = true;
		m_active = false;
		return;
	}

	const Step& step = m_steps[m_currentStep];
	int32_t totalChars = static_cast<int32_t>(step.text.size());

	if (!m_fullyRevealed) {
		// タイプライター進行
		m_typeTimer += deltaTime;
		while (m_typeTimer >= step.typeSpeed && m_revealCount < totalChars) {
			m_typeTimer -= step.typeSpeed;
			++m_revealCount;
		}

		if (m_revealCount >= totalChars) {
			m_fullyRevealed = true;
			m_waitTimer = 0.0f;
		}

		// スキップで即全文表示
		if (skipPressed && !m_fullyRevealed) {
			m_revealCount = totalChars;
			m_fullyRevealed = true;
			m_waitTimer = 0.0f;
			return;
		}
	} else {
		// 全文表示後の待機
		m_waitTimer += deltaTime;

		// キー入力 or 時間経過で次のステップへ
		if (skipPressed || m_waitTimer >= step.displayDuration) {
			++m_currentStep;
			m_revealCount = 0;
			m_typeTimer = 0.0f;
			m_fullyRevealed = false;
			m_waitTimer = 0.0f;

			if (m_currentStep >= static_cast<int32_t>(m_steps.size())) {
				m_finished = true;
				m_active = false;
			}
		}
	}
}

void SubtitleManager::Draw() {
	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);
	constexpr float scale = 2.0f;
	constexpr float bottomMargin = 80.0f;
	constexpr float bgPadding = 12.0f;

	if (m_font) {
		m_font->BeginDraw();
	}

	// チュートリアル字幕
	if (m_active && !m_finished && m_font &&
		m_currentStep < static_cast<int32_t>(m_steps.size())) {

		const Step& step = m_steps[m_currentStep];

		float textWidth = m_font->MeasureTextWidth(step.text, scale);
		float textHeight = m_font->GetLineHeight() * scale;

		float textX = (screenW - textWidth) * 0.5f;
		float textY = screenH - bottomMargin - textHeight;

		// 背景帯（4:3コンテンツ幅960pxに合わせて中央配置）
		constexpr float kBgW = 960.0f;
		constexpr float kBgX = (1280.0f - kBgW) * 0.5f; // 160px
		if (m_bgSprite && m_spriteCommon) {
			m_spriteCommon->CommonDraw();
			m_bgSprite->SetPosition({kBgX, textY - bgPadding});
			m_bgSprite->SetSize({kBgW, textHeight + bgPadding * 2.0f});
			m_bgSprite->setColor({0.0f, 0.0f, 0.0f, 0.6f});
			m_bgSprite->Update();
			m_bgSprite->Draw();
		}

		m_spriteCommon->CommonDraw();
		m_font->RenderText(
			step.text,
			{textX, textY},
			scale,
			{1.0f, 1.0f, 1.0f, 1.0f},
			m_revealCount
		);
	}

	// フェードヒント
	if (m_hintActive && m_font) {
		// フェードイン → 表示 → フェードアウト
		float alpha = 1.0f;
		if (m_hintTimer < HINT_FADE_TIME) {
			alpha = m_hintTimer / HINT_FADE_TIME;
		} else if (m_hintTimer > m_hintDuration - HINT_FADE_TIME) {
			alpha = (m_hintDuration - m_hintTimer) / HINT_FADE_TIME;
		}
		alpha = std::clamp(alpha, 0.0f, 1.0f);

		constexpr float hintScale = 2.5f;
		float textWidth = m_font->MeasureTextWidth(m_hintText, hintScale);
		float textHeight = m_font->GetLineHeight() * hintScale;

		float textX = (screenW - textWidth) * 0.5f;
		float textY = screenH * 0.45f - textHeight * 0.5f;

		// 背景帯
		if (m_bgSprite && m_spriteCommon) {
			m_spriteCommon->CommonDraw();
			m_bgSprite->SetPosition({0.0f, textY - bgPadding});
			m_bgSprite->SetSize({screenW, textHeight + bgPadding * 2.0f});
			m_bgSprite->setColor({0.0f, 0.0f, 0.0f, 0.4f * alpha});
			m_bgSprite->Update();
			m_bgSprite->Draw();
		}

		m_spriteCommon->CommonDraw();
		m_font->RenderText(
			m_hintText,
			{textX, textY},
			hintScale,
			{1.0f, 1.0f, 1.0f, alpha}
		);
	}
}

void SubtitleManager::ShowHint(const std::wstring& text, float duration) {
	m_hintText = text;
	m_hintDuration = duration;
	m_hintTimer = 0.0f;
	m_hintActive = true;
}
