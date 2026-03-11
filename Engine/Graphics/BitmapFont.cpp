#include "pch.h"
#include "BitmapFont.h"
#include "GraphicsDevice.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace UnoEngine {

bool BitmapFont::LoadFromFile(const std::string& fntFilePath) {
    fontDirectory_ = std::filesystem::path(fntFilePath).parent_path().string();
    if (!fontDirectory_.empty() && fontDirectory_.back() != '/' && fontDirectory_.back() != '\\') {
        fontDirectory_ += '/';
    }
    return ParseFntFile(fntFilePath);
}

void BitmapFont::LoadTextures(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList) {
    pageTextures_.clear();
    pageTextures_.reserve(pageFiles_.size());

    for (const auto& pageFile : pageFiles_) {
        auto texture = std::make_unique<Texture2D>();
        std::string fullPath = fontDirectory_ + pageFile;

        std::wstring wpath(fullPath.begin(), fullPath.end());
        uint32 srvIndex = graphics->AllocateSRVIndex();
        texture->LoadFromFile(graphics, commandList, wpath, srvIndex);
        pageTextures_.push_back(std::move(texture));
    }
}

const GlyphInfo* BitmapFont::GetGlyph(uint32 charCode) const {
    auto it = glyphs_.find(charCode);
    if (it != glyphs_.end()) {
        return &it->second;
    }
    return nullptr;
}

float BitmapFont::GetKerning(uint32 first, uint32 second) const {
    auto it = kernings_.find(MakeKerningKey(first, second));
    if (it != kernings_.end()) {
        return it->second;
    }
    return 0.0f;
}

const Texture2D* BitmapFont::GetPageTexture(uint32 page) const {
    if (page < pageTextures_.size()) {
        return pageTextures_[page].get();
    }
    return nullptr;
}

float BitmapFont::MeasureWidth(const std::string& text) const {
    float width = 0.0f;
    uint32 prevChar = 0;

    // UTF-8 decode
    const auto* bytes = reinterpret_cast<const uint8*>(text.data());
    size_t i = 0;
    const size_t len = text.size();

    while (i < len) {
        uint32 cp = 0;
        uint8 b = bytes[i];

        if (b < 0x80) {
            cp = b;
            i += 1;
        } else if ((b & 0xE0) == 0xC0) {
            cp = (b & 0x1F) << 6;
            if (i + 1 < len) cp |= (bytes[i + 1] & 0x3F);
            i += 2;
        } else if ((b & 0xF0) == 0xE0) {
            cp = (b & 0x0F) << 12;
            if (i + 1 < len) cp |= (bytes[i + 1] & 0x3F) << 6;
            if (i + 2 < len) cp |= (bytes[i + 2] & 0x3F);
            i += 3;
        } else if ((b & 0xF8) == 0xF0) {
            cp = (b & 0x07) << 18;
            if (i + 1 < len) cp |= (bytes[i + 1] & 0x3F) << 12;
            if (i + 2 < len) cp |= (bytes[i + 2] & 0x3F) << 6;
            if (i + 3 < len) cp |= (bytes[i + 3] & 0x3F);
            i += 4;
        } else {
            i += 1;
            continue;
        }

        if (prevChar != 0) {
            width += GetKerning(prevChar, cp);
        }

        const auto* glyph = GetGlyph(cp);
        if (glyph) {
            width += glyph->xAdvance;
        }

        prevChar = cp;
    }

    return width;
}

// BMFont text format parser
// Handles: info, common, page, char, kerning lines
bool BitmapFont::ParseFntFile(const std::string& fntFilePath) {
    std::ifstream file(fntFilePath);
    if (!file.is_open()) {
        return false;
    }

    auto extractValue = [](const std::string& token, const std::string& key) -> std::optional<std::string> {
        if (token.substr(0, key.size() + 1) == key + "=") {
            std::string val = token.substr(key.size() + 1);
            // Strip quotes
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }
            return val;
        }
        return std::nullopt;
    };

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string tag;
        stream >> tag;

        if (tag == "common") {
            std::string token;
            while (stream >> token) {
                if (auto v = extractValue(token, "lineHeight")) lineHeight_ = std::stof(*v);
                else if (auto v2 = extractValue(token, "base")) base_ = std::stof(*v2);
                else if (auto v3 = extractValue(token, "scaleW")) scaleW_ = std::stof(*v3);
                else if (auto v4 = extractValue(token, "scaleH")) scaleH_ = std::stof(*v4);
                else if (auto v5 = extractValue(token, "pages")) pageFiles_.resize(std::stoi(*v5));
            }
        } else if (tag == "page") {
            uint32 pageId = 0;
            std::string pageFile;
            std::string token;
            while (stream >> token) {
                if (auto v = extractValue(token, "id")) pageId = std::stoi(*v);
                else if (auto v2 = extractValue(token, "file")) pageFile = *v2;
            }
            if (pageId < pageFiles_.size()) {
                pageFiles_[pageId] = pageFile;
            }
        } else if (tag == "char") {
            GlyphInfo glyph{};
            std::string token;
            while (stream >> token) {
                if (auto v = extractValue(token, "id")) glyph.id = std::stoi(*v);
                else if (auto v2 = extractValue(token, "x")) glyph.x = std::stof(*v2);
                else if (auto v3 = extractValue(token, "y")) glyph.y = std::stof(*v3);
                else if (auto v4 = extractValue(token, "width")) glyph.width = std::stof(*v4);
                else if (auto v5 = extractValue(token, "height")) glyph.height = std::stof(*v5);
                else if (auto v6 = extractValue(token, "xoffset")) glyph.xOffset = std::stof(*v6);
                else if (auto v7 = extractValue(token, "yoffset")) glyph.yOffset = std::stof(*v7);
                else if (auto v8 = extractValue(token, "xadvance")) glyph.xAdvance = std::stof(*v8);
                else if (auto v9 = extractValue(token, "page")) glyph.page = std::stoi(*v9);
            }
            glyphs_[glyph.id] = glyph;
        } else if (tag == "kerning") {
            uint32 first = 0, second = 0;
            float amount = 0.0f;
            std::string token;
            while (stream >> token) {
                if (auto v = extractValue(token, "first")) first = std::stoi(*v);
                else if (auto v2 = extractValue(token, "second")) second = std::stoi(*v2);
                else if (auto v3 = extractValue(token, "amount")) amount = std::stof(*v3);
            }
            kernings_[MakeKerningKey(first, second)] = amount;
        }
    }

    return !glyphs_.empty();
}

} // namespace UnoEngine
