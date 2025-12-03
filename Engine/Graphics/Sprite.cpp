#include "Sprite.h"
#include "GraphicsDevice.h"


namespace UnoEngine {

struct SpriteVertex {
    float x, y;
    float u, v;
};

void Sprite::LoadTexture(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                        const std::wstring& filepath, uint32 srvIndex) {
    texture_ = std::make_unique<Texture2D>();
    texture_->LoadFromFile(graphics, commandList, filepath, srvIndex);
}

void Sprite::Initialize(GraphicsDevice* graphics) {
    auto* device = graphics->GetDevice();

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(SpriteVertex) * 6;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(
        device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer_)
        ),
        "Failed to create sprite vertex buffer"
    );

    vertexBuffer_->Map(0, nullptr, &mappedVertexData_);

    bufferDesc.Width = 256;
    ThrowIfFailed(
        device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&colorBuffer_)
        ),
        "Failed to create sprite color buffer"
    );

    colorBuffer_->Map(0, nullptr, &mappedColorData_);

    screenWidth_ = 1280;
    screenHeight_ = 720;
}

Vector2 Sprite::GetSize() const {
    if (!texture_) {
        return Vector2::Zero();
    }
    return Vector2(
        static_cast<float>(texture_->GetWidth()) * scale_.GetX(),
        static_cast<float>(texture_->GetHeight()) * scale_.GetY()
    );
}

Vector2 Sprite::GetAnchorOffset() const {
    if (!texture_) {
        return Vector2::Zero();
    }

    const float width = static_cast<float>(texture_->GetWidth()) * scale_.GetX();
    const float height = static_cast<float>(texture_->GetHeight()) * scale_.GetY();

    switch (anchor_) {
        case SpriteAnchor::TopLeft:     return Vector2(0.0f, 0.0f);
        case SpriteAnchor::TopCenter:   return Vector2(width * 0.5f, 0.0f);
        case SpriteAnchor::TopRight:    return Vector2(width, 0.0f);
        case SpriteAnchor::MiddleLeft:  return Vector2(0.0f, height * 0.5f);
        case SpriteAnchor::Center:      return Vector2(width * 0.5f, height * 0.5f);
        case SpriteAnchor::MiddleRight: return Vector2(width, height * 0.5f);
        case SpriteAnchor::BottomLeft:  return Vector2(0.0f, height);
        case SpriteAnchor::BottomCenter:return Vector2(width * 0.5f, height);
        case SpriteAnchor::BottomRight: return Vector2(width, height);
        default:                        return Vector2::Zero();
    }
}

void Sprite::Draw(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                 ID3D12PipelineState* pipelineState, ID3D12RootSignature* rootSignature) {
    if (!texture_ || !vertexBuffer_) {
        return;
    }

    const float width = static_cast<float>(texture_->GetWidth()) * scale_.GetX();
    const float height = static_cast<float>(texture_->GetHeight()) * scale_.GetY();
    const Vector2 anchor = GetAnchorOffset();
    const float left = position_.GetX() - anchor.GetX();
    const float top = position_.GetY() - anchor.GetY();

    const float ndcLeft = (left / static_cast<float>(screenWidth_)) * 2.0f - 1.0f;
    const float ndcRight = ((left + width) / static_cast<float>(screenWidth_)) * 2.0f - 1.0f;
    const float ndcTop = 1.0f - (top / static_cast<float>(screenHeight_)) * 2.0f;
    const float ndcBottom = 1.0f - ((top + height) / static_cast<float>(screenHeight_)) * 2.0f;

    SpriteVertex vertices[6] = {
        { ndcLeft,  ndcTop,    0.0f, 0.0f },
        { ndcRight, ndcTop,    1.0f, 0.0f },
        { ndcLeft,  ndcBottom, 0.0f, 1.0f },
        { ndcRight, ndcTop,    1.0f, 0.0f },
        { ndcRight, ndcBottom, 1.0f, 1.0f },
        { ndcLeft,  ndcBottom, 0.0f, 1.0f }
    };

    memcpy(mappedVertexData_, vertices, sizeof(vertices));

    float colorData[4] = { color_.GetX(), color_.GetY(), color_.GetZ(), color_.GetW() };
    memcpy(mappedColorData_, colorData, sizeof(colorData));

    commandList->SetPipelineState(pipelineState);
    commandList->SetGraphicsRootSignature(rootSignature);

    ID3D12DescriptorHeap* heaps[] = { graphics->GetSRVHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = graphics->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
    srvHandle.ptr += texture_->GetSRVIndex() * graphics->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    commandList->SetGraphicsRootDescriptorTable(1, srvHandle);

    commandList->SetGraphicsRootConstantBufferView(0, colorBuffer_->GetGPUVirtualAddress());

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vbView.SizeInBytes = sizeof(vertices);
    vbView.StrideInBytes = sizeof(SpriteVertex);

    commandList->IASetVertexBuffers(0, 1, &vbView);
    commandList->DrawInstanced(6, 1, 0, 0);
}

} // namespace UnoEngine
