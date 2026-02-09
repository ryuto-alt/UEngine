#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "../../Engine/Math/Mymath.h"

class Sprite;
class SpriteCommon;
class DirectXCommon;
class SrvManager;

class BitmapFont {
public:
	BitmapFont() = default;
	~BitmapFont() = default;

	void Initialize(SpriteCommon* spriteCommon, const std::string& fntFilePath);

	// charCountで表示文字数を制限（タイプライター用）
	// -1 = 全文字表示
	void RenderText(
		const std::wstring& text,
		const Vector2& position,
		float scale = 1.0f,
		const Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f},
		int32_t charCount = -1
	);

	// 文字列の描画幅を計算
	float MeasureTextWidth(const std::wstring& text, float scale = 1.0f) const;

	float GetLineHeight() const { return static_cast<float>(m_lineHeight); }

private:
	struct Glyph {
		uint16_t x = 0;
		uint16_t y = 0;
		uint16_t width = 0;
		uint16_t height = 0;
		int16_t xOffset = 0;
		int16_t yOffset = 0;
		uint16_t xAdvance = 0;
	};

	void ParseFntFile(const std::string& fntFilePath);
	Sprite* AcquireSprite();
	void ResetPool();

	SpriteCommon* m_spriteCommon = nullptr;
	std::string m_atlasTexturePath;

	std::unordered_map<uint32_t, Glyph> m_glyphs;
	int32_t m_lineHeight = 0;
	int32_t m_base = 0;

	static constexpr int32_t MAX_SPRITES = 128;
	std::vector<std::unique_ptr<Sprite>> m_spritePool;
	int32_t m_nextSprite = 0;
};
