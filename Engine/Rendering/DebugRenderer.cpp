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

    // グリッドシェーダーとパイプライン
    Shader gridVS, gridPS;
    gridVS.CompileFromFile(L"Shaders/InfiniteGridVS.hlsl", ShaderStage::Vertex);
    gridPS.CompileFromFile(L"Shaders/InfiniteGridPS.hlsl", ShaderStage::Pixel);

    gridPipeline_ = MakeUnique<InfiniteGridPipeline>();
    gridPipeline_->Initialize(device, gridVS, gridPS);
    gridConstantsBuffer_.Create(device);

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
        return;
    }

    const auto& bones = skeleton->GetBones();
    const uint32 boneCount = skeleton->GetBoneCount();

    if (localTransforms.size() != boneCount) {
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

void DebugRenderer::RenderGrid(
    ID3D12GraphicsCommandList* cmdList,
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projectionMatrix,
    const Vector3& cameraPos
) {
    if (!showGrid_) return;

    Matrix4x4 vp = viewMatrix * projectionMatrix;
    Matrix4x4 invVP = vp.Inverse();

    GridConstantsCB cb;
    cb.invViewProj = invVP.Transpose();
    cb.cameraPos = cameraPos;
    cb.gridHeight = gridHeight_;
    cb.viewProj = vp.Transpose();
    gridConstantsBuffer_.Update(cb);

    cmdList->SetPipelineState(gridPipeline_->GetPipelineState());
    cmdList->SetGraphicsRootSignature(gridPipeline_->GetRootSignature());
    cmdList->SetGraphicsRootConstantBufferView(0, gridConstantsBuffer_.GetGPUAddress());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    cmdList->DrawInstanced(4, 1, 0, 0);
}

void DebugRenderer::AddCameraFrustum(const Vector3 nearCorners[4], const Vector3 farCorners[4], const Vector4& color) {
    // Near plane edges
    AddLine(nearCorners[0], nearCorners[1], color);  // bottom
    AddLine(nearCorners[1], nearCorners[2], color);  // right
    AddLine(nearCorners[2], nearCorners[3], color);  // top
    AddLine(nearCorners[3], nearCorners[0], color);  // left

    // Far plane edges
    AddLine(farCorners[0], farCorners[1], color);
    AddLine(farCorners[1], farCorners[2], color);
    AddLine(farCorners[2], farCorners[3], color);
    AddLine(farCorners[3], farCorners[0], color);

    // Connecting edges (near to far)
    AddLine(nearCorners[0], farCorners[0], color);
    AddLine(nearCorners[1], farCorners[1], color);
    AddLine(nearCorners[2], farCorners[2], color);
    AddLine(nearCorners[3], farCorners[3], color);
}

void DebugRenderer::AddCameraIcon(const Vector3& position, const Vector3& forward, const Vector3& up, float scale, const Vector4& color) {
    // カメラ形状を描画（Unityスタイルのワイヤーフレームカメラ）
    Vector3 right = up.Cross(forward).Normalize();

    // カメラボディの寸法
    float bodyW = scale * 0.5f;
    float bodyH = scale * 0.35f;
    float bodyD = scale * 0.6f;
    float lensR = scale * 0.2f;
    float lensL = scale * 0.3f;

    // ボディの中心はカメラ位置
    Vector3 bodyCenter = position;

    // ボディの8つの角
    Vector3 corners[8];
    corners[0] = bodyCenter - right * bodyW - up * bodyH - forward * bodyD; // 後左下
    corners[1] = bodyCenter + right * bodyW - up * bodyH - forward * bodyD; // 後右下
    corners[2] = bodyCenter + right * bodyW + up * bodyH - forward * bodyD; // 後右上
    corners[3] = bodyCenter - right * bodyW + up * bodyH - forward * bodyD; // 後左上
    corners[4] = bodyCenter - right * bodyW - up * bodyH + forward * bodyD; // 前左下
    corners[5] = bodyCenter + right * bodyW - up * bodyH + forward * bodyD; // 前右下
    corners[6] = bodyCenter + right * bodyW + up * bodyH + forward * bodyD; // 前右上
    corners[7] = bodyCenter - right * bodyW + up * bodyH + forward * bodyD; // 前左上

    // ボディの辺を描画
    // 後面
    AddLine(corners[0], corners[1], color);
    AddLine(corners[1], corners[2], color);
    AddLine(corners[2], corners[3], color);
    AddLine(corners[3], corners[0], color);

    // 前面
    AddLine(corners[4], corners[5], color);
    AddLine(corners[5], corners[6], color);
    AddLine(corners[6], corners[7], color);
    AddLine(corners[7], corners[4], color);

    // 接続辺
    AddLine(corners[0], corners[4], color);
    AddLine(corners[1], corners[5], color);
    AddLine(corners[2], corners[6], color);
    AddLine(corners[3], corners[7], color);

    // レンズ（前に突き出た円錐形状）
    Vector3 lensBase = bodyCenter + forward * bodyD;
    Vector3 lensTip = lensBase + forward * lensL;

    // 簡易的なレンズ表現（四角形ベース）
    Vector3 lensCorners[4];
    lensCorners[0] = lensBase - right * lensR - up * lensR;
    lensCorners[1] = lensBase + right * lensR - up * lensR;
    lensCorners[2] = lensBase + right * lensR + up * lensR;
    lensCorners[3] = lensBase - right * lensR + up * lensR;

    // レンズの輪郭
    AddLine(lensCorners[0], lensCorners[1], color);
    AddLine(lensCorners[1], lensCorners[2], color);
    AddLine(lensCorners[2], lensCorners[3], color);
    AddLine(lensCorners[3], lensCorners[0], color);

    // レンズの先端への線
    AddLine(lensCorners[0], lensTip, color);
    AddLine(lensCorners[1], lensTip, color);
    AddLine(lensCorners[2], lensTip, color);
    AddLine(lensCorners[3], lensTip, color);
}

} // namespace UnoEngine
