#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "../Math/Vector.h"
#include "Texture2D.h"
#include "D3D12Common.h"
#include <memory>
#include <string>

namespace UnoEngine {

class GraphicsDevice;

enum class SpriteAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    Center,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

class Sprite : public NonCopyable {
public:
    Sprite() = default;
    ~Sprite() = default;
    Sprite(Sprite&&) = default;
    Sprite& operator=(Sprite&&) = default;

    void LoadTexture(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                    const std::wstring& filepath, uint32 srvIndex);

    void SetPosition(float x, float y) { position_ = Vector2(x, y); }
    void SetScale(float x, float y) { scale_ = Vector2(x, y); }
    void SetScale(float uniform) { scale_ = Vector2(uniform, uniform); }
    void SetAnchor(SpriteAnchor anchor) { anchor_ = anchor; }
    void SetColor(float r, float g, float b, float a = 1.0f) { color_ = Vector4(r, g, b, a); }
    void SetAlpha(float alpha) { color_.SetW(alpha); }

    Vector2 GetPosition() const { return position_; }
    Vector2 GetScale() const { return scale_; }
    Vector2 GetSize() const;
    Vector4 GetColor() const { return color_; }
    const Texture2D* GetTexture() const { return texture_.get(); }

    void Initialize(GraphicsDevice* graphics);
    void Draw(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
             ID3D12PipelineState* pipelineState, ID3D12RootSignature* rootSignature);

private:
    Vector2 GetAnchorOffset() const;

    std::unique_ptr<Texture2D> texture_;
    ComPtr<ID3D12Resource> vertexBuffer_;
    ComPtr<ID3D12Resource> colorBuffer_;
    void* mappedVertexData_ = nullptr;
    void* mappedColorData_ = nullptr;
    Vector2 position_ = Vector2::Zero();
    Vector2 scale_ = Vector2(1.0f, 1.0f);
    Vector4 color_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    SpriteAnchor anchor_ = SpriteAnchor::TopLeft;
    uint32 screenWidth_ = 0;
    uint32 screenHeight_ = 0;
};

} // namespace UnoEngine
