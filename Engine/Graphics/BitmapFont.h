#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "Texture2D.h"
#include "D3D12Common.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace UnoEngine {

class GraphicsDevice;

struct GlyphInfo {
    uint32 id = 0;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float xOffset = 0.0f;
    float yOffset = 0.0f;
    float xAdvance = 0.0f;
    uint32 page = 0;
};

struct KerningPair {
    uint32 first = 0;
    uint32 second = 0;
    float amount = 0.0f;
};

class BitmapFont : public NonCopyable {
public:
    BitmapFont() = default;
    ~BitmapFont() = default;
    BitmapFont(BitmapFont&&) = default;
    BitmapFont& operator=(BitmapFont&&) = default;

    bool LoadFromFile(const std::string& fntFilePath);
    void LoadTextures(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList);

    const GlyphInfo* GetGlyph(uint32 charCode) const;
    float GetKerning(uint32 first, uint32 second) const;

    float GetLineHeight() const { return lineHeight_; }
    float GetBase() const { return base_; }
    float GetScaleW() const { return scaleW_; }
    float GetScaleH() const { return scaleH_; }
    uint32 GetPageCount() const { return static_cast<uint32>(pageTextures_.size()); }
    const Texture2D* GetPageTexture(uint32 page) const;

    float MeasureWidth(const std::string& text) const;
    float MeasureHeight() const { return lineHeight_; }

private:
    bool ParseFntFile(const std::string& fntFilePath);
    static uint64 MakeKerningKey(uint32 first, uint32 second) { return (static_cast<uint64>(first) << 32) | second; }

    float lineHeight_ = 0.0f;
    float base_ = 0.0f;
    float scaleW_ = 0.0f;
    float scaleH_ = 0.0f;
    std::string fontDirectory_;

    std::unordered_map<uint32, GlyphInfo> glyphs_;
    std::unordered_map<uint64, float> kernings_;
    std::vector<std::string> pageFiles_;
    std::vector<std::unique_ptr<Texture2D>> pageTextures_;
};

} // namespace UnoEngine
