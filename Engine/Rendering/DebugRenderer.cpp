#include "DebugRenderer.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Shader.h"
#include "../Animation/Skeleton.h"
#include "../Core/Logger.h"

namespace UnoEngine {

void DebugRenderer::Initialize(GraphicsDevice* graphics) {
    graphics_ = graphics;
    auto device = graphics->GetDevice();

    // シェーダーロード
    Shader vertexShader, pixelShader;
    vertexShader.CompileFromFile(L"Shaders/DebugLineVS.hlsl", ShaderStage::Vertex);
    pixelShader.CompileFromFile(L"Shaders/DebugLinePS.hlsl", ShaderStage::Pixel);

    // パイプライン作成
    pipeline_ = MakeUnique<DebugLinePipeline>();
    pipeline_->Initialize(device, vertexShader, pixelShader);

    // 定数バッファ作成
    transformBuffer_.Create(device);

    // 動的頂点バッファ作成
    CreateDynamicVertexBuffer(device);

    Logger::Info("デバッグレンダラー初期化完了");
}

void DebugRenderer::CreateDynamicVertexBuffer(ID3D12Device* device) {
    const uint32 bufferSize = MAX_VERTICES * sizeof(DebugLineVertex);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = bufferSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(
        device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer_)
        ),
        "Failed to create debug vertex buffer"
    );

    // 常時マップ
    ThrowIfFailed(
        vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_)),
        "Failed to map debug vertex buffer"
    );

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = bufferSize;
    vertexBufferView_.StrideInBytes = sizeof(DebugLineVertex);
}

void DebugRenderer::BeginFrame() {
    vertices_.clear();
}

void DebugRenderer::AddLine(const Vector3& start, const Vector3& end, const Vector4& color) {
    if (vertices_.size() + 2 > MAX_VERTICES) {
        return;
    }

    DebugLineVertex v1, v2;
    v1.position[0] = start.GetX();
    v1.position[1] = start.GetY();
    v1.position[2] = start.GetZ();
    v1.color[0] = color.GetX();
    v1.color[1] = color.GetY();
    v1.color[2] = color.GetZ();
    v1.color[3] = color.GetW();

    v2.position[0] = end.GetX();
    v2.position[1] = end.GetY();
    v2.position[2] = end.GetZ();
    v2.color[0] = color.GetX();
    v2.color[1] = color.GetY();
    v2.color[2] = color.GetZ();
    v2.color[3] = color.GetW();

    vertices_.push_back(v1);
    vertices_.push_back(v2);
}

void DebugRenderer::DrawBones(
    const Skeleton* skeleton,
    const std::vector<Matrix4x4>& localTransforms,
    const Matrix4x4& worldMatrix
) {
    if (!showBones_ || !skeleton) {
        Logger::Info("[DebugRenderer] DrawBones: showBones={}, skeleton={}", showBones_, skeleton != nullptr);
        return;
    }

    const auto& bones = skeleton->GetBones();
    const uint32 boneCount = skeleton->GetBoneCount();

    Logger::Info("[DebugRenderer] DrawBones: boneCount={}, localTransforms.size()={}", boneCount, localTransforms.size());

    if (localTransforms.size() != boneCount) {
        Logger::Info("[DebugRenderer] localTransforms size mismatch: {} vs {}", localTransforms.size(), boneCount);
        return;
    }

    // グローバル変換を計算
    std::vector<Matrix4x4> globalTransforms(boneCount);

    for (uint32 i = 0; i < boneCount; ++i) {
        const Bone& bone = bones[i];

        if (bone.parentIndex == INVALID_BONE_INDEX) {
            globalTransforms[i] = localTransforms[i];
        } else {
            globalTransforms[i] = localTransforms[i] * globalTransforms[bone.parentIndex];
        }
    }

    // ボーンの線を描画
    for (uint32 i = 0; i < boneCount; ++i) {
        const Bone& bone = bones[i];

        // ボーン位置（行列の平行移動成分をワールド座標に変換）
        Matrix4x4 boneWorldMatrix = globalTransforms[i] * worldMatrix;
        Vector3 bonePos = boneWorldMatrix.TransformPoint(Vector3::Zero());

        // 親ボーンがあれば線を引く
        if (bone.parentIndex != INVALID_BONE_INDEX) {
            Matrix4x4 parentWorldMatrix = globalTransforms[bone.parentIndex] * worldMatrix;
            Vector3 parentPos = parentWorldMatrix.TransformPoint(Vector3::Zero());

            AddLine(parentPos, bonePos, boneColor_);
        }

        // 関節を示す小さな十字を描画
        const float jointSize = 0.02f;
        Vector3 right = boneWorldMatrix.TransformDirection(Vector3::UnitX()) * jointSize;
        Vector3 up = boneWorldMatrix.TransformDirection(Vector3::UnitY()) * jointSize;
        Vector3 forward = boneWorldMatrix.TransformDirection(Vector3::UnitZ()) * jointSize;

        AddLine(bonePos - right, bonePos + right, jointColor_);
        AddLine(bonePos - up, bonePos + up, jointColor_);
        AddLine(bonePos - forward, bonePos + forward, jointColor_);
    }
}

void DebugRenderer::AddSphere(const Vector3& center, float radius, const Vector4& color, int segments) {
    // 簡易的な球体（3つの円環）
    const float angleStep = DirectX::XM_2PI / segments;

    // XY平面
    for (int i = 0; i < segments; ++i) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;

        Vector3 p1(center.GetX() + radius * std::cos(angle1),
                   center.GetY() + radius * std::sin(angle1),
                   center.GetZ());
        Vector3 p2(center.GetX() + radius * std::cos(angle2),
                   center.GetY() + radius * std::sin(angle2),
                   center.GetZ());
        AddLine(p1, p2, color);
    }

    // XZ平面
    for (int i = 0; i < segments; ++i) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;

        Vector3 p1(center.GetX() + radius * std::cos(angle1),
                   center.GetY(),
                   center.GetZ() + radius * std::sin(angle1));
        Vector3 p2(center.GetX() + radius * std::cos(angle2),
                   center.GetY(),
                   center.GetZ() + radius * std::sin(angle2));
        AddLine(p1, p2, color);
    }

    // YZ平面
    for (int i = 0; i < segments; ++i) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;

        Vector3 p1(center.GetX(),
                   center.GetY() + radius * std::cos(angle1),
                   center.GetZ() + radius * std::sin(angle1));
        Vector3 p2(center.GetX(),
                   center.GetY() + radius * std::cos(angle2),
                   center.GetZ() + radius * std::sin(angle2));
        AddLine(p1, p2, color);
    }
}

void DebugRenderer::UpdateVertexBuffer() {
    if (vertices_.empty() || !mappedVertices_) {
        return;
    }

    const uint32 copySize = static_cast<uint32>(vertices_.size()) * sizeof(DebugLineVertex);
    memcpy(mappedVertices_, vertices_.data(), copySize);
}

void DebugRenderer::Render(
    ID3D12GraphicsCommandList* cmdList,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projectionMatrix
) {
    Logger::Info("[DebugRenderer] Render: vertices.size()={}", vertices_.size());
    
    if (vertices_.empty()) {
        return;
    }

    // 頂点バッファ更新
    UpdateVertexBuffer();

    // 定数バッファ更新（HLSLはcolumn-majorなのでTranspose必要）
    DebugTransformCB cb;
    Matrix4x4 vp = viewMatrix * projectionMatrix;
    cb.viewProjection = vp.Transpose();
    transformBuffer_.Update(cb);

    // パイプライン設定
    cmdList->SetPipelineState(pipeline_->GetPipelineState());
    cmdList->SetGraphicsRootSignature(pipeline_->GetRootSignature());

    // 定数バッファ設定
    cmdList->SetGraphicsRootConstantBufferView(0, transformBuffer_.GetGPUAddress());

    // 頂点バッファ設定
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    // 描画
    cmdList->DrawInstanced(static_cast<uint32>(vertices_.size()), 1, 0, 0);
}

} // namespace UnoEngine
