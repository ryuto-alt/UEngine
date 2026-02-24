#include "BitmapFont.h"
#include "../../Engine/Graphics/Sprite.h"
#include "../../Engine/Graphics/SpriteCommon.h"
#include "../../Engine/Utility/StringUtility.h"
#include "../../Engine/Graphics/TextureManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>

// Helper: open ifstream with UTF-8 narrow path on Windows
static std::ifstream OpenUtf8(const std::string& utf8Path) {
	std::wstring widePath = StringUtility::ConvertString(utf8Path);
	return std::ifstream(widePath);
}

// Helper: get parent directory from UTF-8 path (returns UTF-8)
static std::string GetParentDirUtf8(const std::string& utf8Path) {
	std::wstring widePath = StringUtility::ConvertString(utf8Path);
	std::wstring wideParent = std::filesystem::path(widePath).parent_path().wstring();
	return StringUtility::ConvertString(wideParent) + "/";
}

void BitmapFont::Initialize(SpriteCommon* spriteCommon, const std::string& fntFilePath) {
	m_spriteCommon = spriteCommon;

	ParseFntFile(fntFilePath);

	// テクスチャだけ事前にロード（1回のみ、キャッシュされる）
	for (int32_t p = 0; p < m_pageCount; ++p) {
		if (!m_atlasTexturePaths[p].empty()) {
			TextureManager::GetInstance()->LoadTexture(m_atlasTexturePaths[p]);
		}
	}

	// Spriteプールは空で初期化（遅延作成）
	m_spritePoolPerPage.resize(m_pageCount);
	m_nextSpritePerPage.resize(m_pageCount, 0);
}

void BitmapFont::ParseFntFile(const std::string& fntFilePath) {
	std::ifstream file = OpenUtf8(fntFilePath);
	if (!file.is_open()) {
		OutputDebugStringA(("BitmapFont: Failed to open " + fntFilePath + "\n").c_str());
		return;
	}

	// .fntファイルの親ディレクトリ (UTF-8)
	std::string parentDir = GetParentDirUtf8(fntFilePath);

	std::string line;
	while (std::getline(file, line)) {
		if (line.substr(0, 6) == "common") {
			std::istringstream ss(line);
			std::string token;
			while (ss >> token) {
				if (token.substr(0, 11) == "lineHeight=") {
					m_lineHeight = std::stoi(token.substr(11));
				} else if (token.substr(0, 5) == "base=") {
					m_base = std::stoi(token.substr(5));
				} else if (token.substr(0, 6) == "pages=") {
					m_pageCount = std::stoi(token.substr(6));
					if (m_pageCount < 1) m_pageCount = 1;
					m_atlasTexturePaths.resize(m_pageCount);
				}
			}
		} else if (line.substr(0, 4) == "page") {
			// page id=0 file="filename.png"
			int pageId = 0;
			std::istringstream ss(line);
			std::string token;
			while (ss >> token) {
				if (token.substr(0, 3) == "id=") {
					pageId = std::stoi(token.substr(3));
				}
			}

			auto pos = line.find("file=\"");
			if (pos != std::string::npos) {
				auto endPos = line.find('\"', pos + 6);
				std::string fileName = line.substr(pos + 6, endPos - (pos + 6));
				if (pageId < m_pageCount) {
					m_atlasTexturePaths[pageId] = parentDir + fileName;
				}
			}
		} else if (line.substr(0, 5) == "char ") {
			Glyph glyph{};
			uint32_t id = 0;

			std::istringstream ss(line);
			std::string token;
			while (ss >> token) {
				if (token.substr(0, 3) == "id=") {
					id = static_cast<uint32_t>(std::stoi(token.substr(3)));
				} else if (token.substr(0, 2) == "x=") {
					glyph.x = static_cast<uint16_t>(std::stoi(token.substr(2)));
				} else if (token.substr(0, 2) == "y=") {
					glyph.y = static_cast<uint16_t>(std::stoi(token.substr(2)));
				} else if (token.substr(0, 6) == "width=") {
					glyph.width = static_cast<uint16_t>(std::stoi(token.substr(6)));
				} else if (token.substr(0, 7) == "height=") {
					glyph.height = static_cast<uint16_t>(std::stoi(token.substr(7)));
				} else if (token.substr(0, 8) == "xoffset=") {
					glyph.xOffset = static_cast<int16_t>(std::stoi(token.substr(8)));
				} else if (token.substr(0, 8) == "yoffset=") {
					glyph.yOffset = static_cast<int16_t>(std::stoi(token.substr(8)));
				} else if (token.substr(0, 9) == "xadvance=") {
					glyph.xAdvance = static_cast<uint16_t>(std::stoi(token.substr(9)));
				} else if (token.substr(0, 5) == "page=") {
					glyph.page = static_cast<uint16_t>(std::stoi(token.substr(5)));
				}
			}
			m_glyphs[id] = glyph;
		}
	}

	// Ensure at least 1 page path exists for backwards compatibility
	if (m_atlasTexturePaths.empty()) {
		m_atlasTexturePaths.push_back("");
		m_pageCount = 1;
	}

	OutputDebugStringA(("BitmapFont: Loaded " + std::to_string(m_glyphs.size()) +
		" glyphs (" + std::to_string(m_pageCount) + " pages) from " + fntFilePath + "\n").c_str());
}

void BitmapFont::BeginDraw() {
	ResetPool();
}

void BitmapFont::RenderText(
	const std::wstring& text,
	const Vector2& position,
	float scale,
	const Vector4& color,
	int32_t charCount
) {

	float cursorX = position.x;
	int32_t rendered = 0;
	int32_t limit = (charCount < 0) ? static_cast<int32_t>(text.size()) : charCount;

	for (int32_t i = 0; i < static_cast<int32_t>(text.size()) && rendered < limit; ++i) {
		uint32_t codepoint = static_cast<uint32_t>(text[i]);

		// サロゲートペアは今回スキップ（BMP範囲で十分）
		auto it = m_glyphs.find(codepoint);
		if (it == m_glyphs.end()) {
			// 未知の文字はスペース分だけ進める
			cursorX += m_lineHeight * scale * 0.5f;
			++rendered;
			continue;
		}

		const Glyph& g = it->second;
		Sprite* sprite = AcquireSprite(g.page);
		if (!sprite) break;

		sprite->SetTextureLeftTop({static_cast<float>(g.x), static_cast<float>(g.y)});
		sprite->SetTextureSize({static_cast<float>(g.width), static_cast<float>(g.height)});
		sprite->SetSize({g.width * scale, g.height * scale});
		sprite->SetPosition({cursorX + g.xOffset * scale, position.y + g.yOffset * scale});
		sprite->setColor(color);
		sprite->Update();
		sprite->Draw();

		cursorX += g.xAdvance * scale;
		++rendered;
	}
}

float BitmapFont::MeasureTextWidth(const std::wstring& text, float scale) const {
	float width = 0.0f;
	for (wchar_t ch : text) {
		auto it = m_glyphs.find(static_cast<uint32_t>(ch));
		if (it != m_glyphs.end()) {
			width += it->second.xAdvance * scale;
		} else {
			width += m_lineHeight * scale * 0.5f;
		}
	}
	return width;
}

Sprite* BitmapFont::AcquireSprite(uint16_t page) {
	if (page >= m_spritePoolPerPage.size()) return nullptr;
	auto& pool = m_spritePoolPerPage[page];
	auto& next = m_nextSpritePerPage[page];

	// プールが足りなければ新しいSpriteを遅延作成
	if (next >= static_cast<int32_t>(pool.size())) {
		if (static_cast<int32_t>(pool.size()) >= MAX_SPRITES_PER_PAGE) {
			return nullptr; // 上限到達
		}
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(m_spriteCommon, m_atlasTexturePaths[page]);
		pool.push_back(std::move(sprite));
	}

	return pool[next++].get();
}

void BitmapFont::ResetPool() {
	for (auto& next : m_nextSpritePerPage) {
		next = 0;
	}
}
