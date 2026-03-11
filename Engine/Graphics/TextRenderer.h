#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "BitmapFont.h"
#include "SpritePipeline.h"
#include "D3D12Common.h"
#include <string>
#include <vector>

namespace UnoEngine {

class GraphicsDevice;

enum class TextAnchor {
    TopLeft,
    TopCenter,
    Center,
    BottomCenter,
    BottomLeft
};

struct TextVertex {
    float x, y;
    float u, v;
};

class TextRenderer : public NonCopyable {
public:
    static constexpr uint32 kMaxGlyphs = 256;

    TextRenderer() = default;
    ~TextRenderer() = default;
    TextRenderer(TextRenderer&&) = default;
    TextRenderer& operator=(TextRenderer&&) = default;

    void Initialize(GraphicsDevice* graphics);
    void SetScreenSize(uint32 width, uint32 height);

    void DrawText(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                  BitmapFont* font, const std::string& text,
                  float x, float y, float scale = 1.0f,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f,
                  TextAnchor anchor = TextAnchor::TopLeft);

    SpritePipeline& GetPipeline() { return pipeline_; }

private:
    void BuildVertices(BitmapFont* font, const std::string& text,
                       float x, float y, float scale, TextAnchor anchor);

    SpritePipeline pipeline_;
    ComPtr<ID3D12Resource> vertexBuffer_;
    ComPtr<ID3D12Resource> colorBuffer_;
    void* mappedVertexData_ = nullptr;
    void* mappedColorData_ = nullptr;
    uint32 screenWidth_ = 1280;
    uint32 screenHeight_ = 720;
    uint32 currentGlyphCount_ = 0;

    // Per-draw glyph batch (page -> vertex range)
    struct GlyphBatch {
        uint32 page = 0;
        uint32 startVertex = 0;
        uint32 vertexCount = 0;
    };
    std::vector<GlyphBatch> batches_;
};

} // namespace UnoEngine
