#pragma once

#include "../Core/Types.h"
#include "../Graphics/D3D12Common.h"
#include "../Graphics/DebugLinePipeline.h"
#include "../Graphics/InfiniteGridPipeline.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include <vector>

namespace UnoEngine {

class GraphicsDevice;
class SkinnedMeshRenderer;
class Skeleton;

// デバッグ描画用定数バッファ
struct DebugTransformCB {
    Matrix4x4 viewProjection;
};

// グリッド描画用定数バッファ
struct GridConstantsCB {
    Matrix4x4 invViewProj;
    Vector3 cameraPos;
    float gridHeight;
    float padding[3];
    Matrix4x4 viewProj;
};

// デバッグレンダラー
class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer() = default;

    // 初期化
    void Initialize(GraphicsDevice* graphics);

    // 描画設定
    void SetShowBones(bool show) { showBones_ = show; }
    bool GetShowBones() const { return showBones_; }

    void SetBoneColor(const Vector4& color) { boneColor_ = color; }
    void SetJointColor(const Vector4& color) { jointColor_ = color; }

    // グリッド設定
    void SetShowGrid(bool show) { showGrid_ = show; }
    bool GetShowGrid() const { return showGrid_; }
    void SetGridHeight(float height) { gridHeight_ = height; }
    float GetGridHeight() const { return gridHeight_; }

    // ライン追加（フレーム内で呼び出し）
    void AddLine(const Vector3& start, const Vector3& end, const Vector4& color);

    // ボーン描画（スキンメッシュレンダラーからボーン情報を収集）
    void DrawBones(
        const Skeleton* skeleton,
        const std::vector<Matrix4x4>& localTransforms,
        const Matrix4x4& worldMatrix
    );

    // 球体描画（関節用）
    void AddSphere(const Vector3& center, float radius, const Vector4& color, int segments = 8);

    // フレーム開始時にクリア
    void BeginFrame();

    // 描画実行
    void Render(
        ID3D12GraphicsCommandList* cmdList,
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projectionMatrix
    );

    // グリッド描画
    void RenderGrid(
        ID3D12GraphicsCommandList* cmdList,
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projectionMatrix,
        const Vector3& cameraPos
    );

private:
    void CreateDynamicVertexBuffer(ID3D12Device* device);
    void UpdateVertexBuffer();

    GraphicsDevice* graphics_ = nullptr;
    UniquePtr<DebugLinePipeline> pipeline_;
    ConstantBuffer<DebugTransformCB> transformBuffer_;

    // グリッド用
    UniquePtr<InfiniteGridPipeline> gridPipeline_;
    ConstantBuffer<GridConstantsCB> gridConstantsBuffer_;

    // 動的頂点バッファ
    ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    DebugLineVertex* mappedVertices_ = nullptr;
    static constexpr uint32 MAX_VERTICES = 65536;

    // フレーム内の頂点データ
    std::vector<DebugLineVertex> vertices_;

    // 設定
    bool showBones_ = true;
    Vector4 boneColor_ = Vector4(0.0f, 1.0f, 0.0f, 1.0f);   // 緑
    Vector4 jointColor_ = Vector4(1.0f, 1.0f, 0.0f, 1.0f);  // 黄
    bool showGrid_ = true;
    float gridHeight_ = 0.0f;
};

} // namespace UnoEngine
