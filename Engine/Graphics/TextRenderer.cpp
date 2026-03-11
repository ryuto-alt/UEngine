#include "pch.h"
#include "TextRenderer.h"
#include "GraphicsDevice.h"
#include "Shader.h"

namespace UnoEngine {

void TextRenderer::Initialize(GraphicsDevice* graphics) {
    auto* device = graphics->GetDevice();

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(TextVertex) * 6 * kMaxGlyphs;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(
        device->CreateCommittedResource(
            &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&vertexBuffer_)),
        "Failed to create text vertex buffer"
    );
    vertexBuffer_->Map(0, nullptr, &mappedVertexData_);

    bufferDesc.Width = 256;
    ThrowIfFailed(
        device->CreateCommittedResource(
            &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&colorBuffer_)),
        "Failed to create text color buffer"
    );
    colorBuffer_->Map(0, nullptr, &mappedColorData_);

    Shader vs, ps;
    vs.CompileFromFile(L"Shaders/Sprite/SpriteVS.hlsl", ShaderStage::Vertex);
    ps.CompileFromFile(L"Shaders/Sprite/SpritePS.hlsl", ShaderStage::Pixel);
    pipeline_.Initialize(device, vs, ps);
}

void TextRenderer::SetScreenSize(uint32 width, uint32 height) {
    screenWidth_ = width;
    screenHeight_ = height;
}

void TextRenderer::BuildVertices(BitmapFont* font, const std::string& text,
                                  float x, float y, float scale, TextAnchor anchor) {
    batches_.clear();
    currentGlyphCount_ = 0;

    // Anchor offset
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    if (anchor != TextAnchor::TopLeft) {
        float textWidth = font->MeasureWidth(text) * scale;
        float textHeight = font->GetLineHeight() * scale;
        switch (anchor) {
            case TextAnchor::TopCenter:    offsetX = -textWidth * 0.5f; break;
            case TextAnchor::Center:       offsetX = -textWidth * 0.5f; offsetY = -textHeight * 0.5f; break;
            case TextAnchor::BottomCenter: offsetX = -textWidth * 0.5f; offsetY = -textHeight; break;
            case TextAnchor::BottomLeft:   offsetY = -textHeight; break;
            default: break;
        }
    }

    float cursorX = x + offsetX;
    float cursorY = y + offsetY;
    uint32 prevChar = 0;

    auto* vertices = static_cast<TextVertex*>(mappedVertexData_);
    const float scaleW = font->GetScaleW();
    const float scaleH = font->GetScaleH();
    const float sw = static_cast<float>(screenWidth_);
    const float sh = static_cast<float>(screenHeight_);

    // UTF-8 decode + vertex generation
    const auto* bytes = reinterpret_cast<const uint8*>(text.data());
    size_t i = 0;
    const size_t len = text.size();

    uint32 currentPage = UINT32_MAX;
    uint32 batchStartVertex = 0;

    while (i < len && currentGlyphCount_ < kMaxGlyphs) {
        uint32 cp = 0;
        uint8 b = bytes[i];

        if (b < 0x80) {
            cp = b; i += 1;
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
            cursorX += font->GetKerning(prevChar, cp) * scale;
        }

        const auto* glyph = font->GetGlyph(cp);
        if (!glyph) {
            prevChar = cp;
            continue;
        }

        // Page change -> new batch
        if (glyph->page != currentPage) {
            if (currentPage != UINT32_MAX && currentGlyphCount_ * 6 > batchStartVertex) {
                batches_.push_back({ currentPage, batchStartVertex, currentGlyphCount_ * 6 - batchStartVertex });
            }
            currentPage = glyph->page;
            batchStartVertex = currentGlyphCount_ * 6;
        }

        float glyphX = cursorX + glyph->xOffset * scale;
        float glyphY = cursorY + glyph->yOffset * scale;
        float glyphW = glyph->width * scale;
        float glyphH = glyph->height * scale;

        // Screen -> NDC
        float ndcLeft   = (glyphX / sw) * 2.0f - 1.0f;
        float ndcRight  = ((glyphX + glyphW) / sw) * 2.0f - 1.0f;
        float ndcTop    = 1.0f - (glyphY / sh) * 2.0f;
        float ndcBottom = 1.0f - ((glyphY + glyphH) / sh) * 2.0f;

        // UV from atlas
        float uvLeft   = glyph->x / scaleW;
        float uvRight  = (glyph->x + glyph->width) / scaleW;
        float uvTop    = glyph->y / scaleH;
        float uvBottom = (glyph->y + glyph->height) / scaleH;

        uint32 vi = currentGlyphCount_ * 6;
        vertices[vi + 0] = { ndcLeft,  ndcTop,    uvLeft,  uvTop };
        vertices[vi + 1] = { ndcRight, ndcTop,    uvRight, uvTop };
        vertices[vi + 2] = { ndcLeft,  ndcBottom, uvLeft,  uvBottom };
        vertices[vi + 3] = { ndcRight, ndcTop,    uvRight, uvTop };
        vertices[vi + 4] = { ndcRight, ndcBottom, uvRight, uvBottom };
        vertices[vi + 5] = { ndcLeft,  ndcBottom, uvLeft,  uvBottom };

        cursorX += glyph->xAdvance * scale;
        prevChar = cp;
        currentGlyphCount_++;
    }

    // Final batch
    if (currentPage != UINT32_MAX && currentGlyphCount_ * 6 > batchStartVertex) {
        batches_.push_back({ currentPage, batchStartVertex, currentGlyphCount_ * 6 - batchStartVertex });
    }
}

void TextRenderer::DrawText(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                            BitmapFont* font, const std::string& text,
                            float x, float y, float scale,
                            float r, float g, float b, float a,
                            TextAnchor anchor) {
    if (!font || text.empty() || !vertexBuffer_) return;

    BuildVertices(font, text, x, y, scale, anchor);
    if (batches_.empty()) return;

    float colorData[4] = { r, g, b, a };
    memcpy(mappedColorData_, colorData, sizeof(colorData));

    commandList->SetPipelineState(pipeline_.GetPipelineState());
    commandList->SetGraphicsRootSignature(pipeline_.GetRootSignature());

    ID3D12DescriptorHeap* heaps[] = { graphics->GetSRVHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetGraphicsRootConstantBufferView(0, colorBuffer_->GetGPUVirtualAddress());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView.SizeInBytes = sizeof(TextVertex) * 6 * currentGlyphCount_;
    vbView.StrideInBytes = sizeof(TextVertex);
    commandList->IASetVertexBuffers(0, 1, &vbView);

    for (const auto& batch : batches_) {
        const auto* texture = font->GetPageTexture(batch.page);
        if (!texture) continue;

        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = graphics->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
        srvHandle.ptr += texture->GetSRVIndex() *
            graphics->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        commandList->SetGraphicsRootDescriptorTable(1, srvHandle);

        commandList->DrawInstanced(batch.vertexCount, 1, batch.startVertex, 0);
    }
}

} // namespace UnoEngine
