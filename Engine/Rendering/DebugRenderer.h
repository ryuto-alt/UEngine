#pragma once

#include "../Core/Types.h"
#include "../Graphics/D3D12Common.h"
#include "../Graphics/DebugLinePipeline.h"
#include "../Graphics/DebugTrianglePipeline.h"
#include "../Graphics/DebugXRayLinePipeline.h"
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

    // 三角形描画（塗りつぶし、NavMesh可視化用）
    void AddTriangle(const Vector3& v0, const Vector3& v1, const Vector3& v2, const Vector4& color);

    // カメラFrustum描画
    void AddCameraFrustum(const Vector3 nearCorners[4], const Vector3 farCorners[4], const Vector4& color);

    // カメラアイコン描画（簡易ワイヤーフレームカメラ形状）
    void AddCameraIcon(const Vector3& position, const Vector3& forward, const Vector3& up, float scale, const Vector4& color);

    // AABB描画（ワイヤーフレームボックス）
    void AddBox(const Vector3& min, const Vector3& max, const Vector4& color);

    // X-Ray描画（深度テスト無効、常に最前面）
    void AddLineXRay(const Vector3& start, const Vector3& end, const Vector4& color);
    void AddSphereXRay(const Vector3& center, float radius, const Vector4& color, int segments = 8);

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
    void UpdateTriangleVertexBuffer();
    void UpdateXRayVertexBuffer();

    GraphicsDevice* graphics_ = nullptr;
    UniquePtr<DebugLinePipeline> pipeline_;
    UniquePtr<DebugTrianglePipeline> trianglePipeline_;
    UniquePtr<DebugXRayLinePipeline> xrayPipeline_;
    ConstantBuffer<DebugTransformCB> transformBuffer_;

    // グリッド用
    UniquePtr<InfiniteGridPipeline> gridPipeline_;
    ConstantBuffer<GridConstantsCB> gridConstantsBuffer_;

    // ライン用動的頂点バッファ
    ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
    DebugLineVertex* mappedVertices_ = nullptr;
    static constexpr uint32 MAX_VERTICES = 65536;

    // 三角形用動的頂点バッファ
    ComPtr<ID3D12Resource> triangleVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW triangleVertexBufferView_ = {};
    DebugLineVertex* mappedTriangleVertices_ = nullptr;
    static constexpr uint32 MAX_TRIANGLE_VERTICES = 65536;

    // X-Ray用動的頂点バッファ
    ComPtr<ID3D12Resource> xrayVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW xrayVertexBufferView_ = {};
    DebugLineVertex* mappedXRayVertices_ = nullptr;
    static constexpr uint32 MAX_XRAY_VERTICES = 16384;

    // フレーム内の頂点データ
    std::vector<DebugLineVertex> vertices_;
    std::vector<DebugLineVertex> triangleVertices_;
    std::vector<DebugLineVertex> xrayVertices_;

    // 設定
#ifdef NDEBUG
    bool showBones_ = false;  // Releaseでは非表示
#else
    bool showBones_ = true;   // Debugでは表示
#endif
    Vector4 boneColor_ = Vector4(0.0f, 1.0f, 0.0f, 1.0f);   // 緑
    Vector4 jointColor_ = Vector4(1.0f, 1.0f, 0.0f, 1.0f);  // 黄
    bool showGrid_ = true;
    float gridHeight_ = 0.0f;
};

} // namespace UnoEngine
