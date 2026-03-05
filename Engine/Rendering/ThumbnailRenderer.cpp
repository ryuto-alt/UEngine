#include "pch.h"
#include "ThumbnailRenderer.h"

#include "../Graphics/GraphicsDevice.h"
#include "../Resource/ResourceLoader.h"
#include "Renderer.h"
#include "LightManager.h"
#include "RenderView.h"
#include "RenderItem.h"
#include "../Math/Matrix.h"
#include "../Math/Quaternion.h"

#include <algorithm>
#include <cfloat>

namespace UnoEngine {

void ThumbnailRenderer::Initialize(GraphicsDevice* graphics, ResourceManager* /*resources*/) {
    graphics_    = graphics;
    initialized_ = true;

    // カメラは ProcessOne でモデルごとに設定する
    camera_.SetPerspective(3.14159265f / 4.0f, 1.0f, 0.1f, 500.0f);
}

D3D12_GPU_DESCRIPTOR_HANDLE ThumbnailRenderer::Request(const std::string& modelPath) {
    if (!initialized_) return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

    auto it = cache_.find(modelPath);
    if (it != cache_.end()) {
        if (it->second.ready) return it->second.rt->GetSRVHandle();
        return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
    }

    // 新規エントリ: RT 作成 + キューに積む
    auto& entry = cache_[modelPath];
    entry.rt    = std::make_unique<RenderTexture>();
    uint32_t srvIdx = graphics_->AllocateSRVIndex();
    entry.rt->Create(graphics_, kSize, kSize, srvIdx);
    entry.ready = false;
    pending_.push_back(modelPath);

    return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
}

bool ThumbnailRenderer::IsReady(const std::string& modelPath) const {
    auto it = cache_.find(modelPath);
    return it != cache_.end() && it->second.ready;
}

void ThumbnailRenderer::PreLoadPending() {
    // BeginFrame前（コマンドリスト閉じた状態）でモデルをキャッシュへロードする。
    // ResourceLoader::LoadModel はコマンドリストを Reset→Close→Execute するため、
    // BeginFrame後に呼ぶと COMMAND_LIST_OPEN エラーになる。
    if (pending_.empty()) return;
    const std::string& path = pending_.front();
    ResourceLoader::LoadModel(path); // キャッシュミス時のみコマンドリストを使用
}

void ThumbnailRenderer::ProcessOne(Renderer* renderer, LightManager* lights, GraphicsDevice* /*graphics*/) {
    if (pending_.empty()) return;

    std::string path = pending_.front();
    pending_.pop_front();

    auto it = cache_.find(path);
    if (it == cache_.end()) return;
    auto& entry = it->second;
    if (entry.ready) return;

    // モデルの全メッシュを取得（キャッシュ済みなら高速）
    auto meshes = ResourceLoader::LoadModel(path);
    if (meshes.empty()) {
        entry.ready = true; // 失敗してもリトライしない
        return;
    }

    // バウンディングボックスを集計してカメラを自動フレーミング
    float bMinX = FLT_MAX, bMinY = FLT_MAX, bMinZ = FLT_MAX;
    float bMaxX = -FLT_MAX, bMaxY = -FLT_MAX, bMaxZ = -FLT_MAX;
    for (auto* mesh : meshes) {
        auto mn = mesh->GetBoundsMin();
        auto mx = mesh->GetBoundsMax();
        bMinX = std::min(bMinX, mn.GetX()); bMinY = std::min(bMinY, mn.GetY()); bMinZ = std::min(bMinZ, mn.GetZ());
        bMaxX = std::max(bMaxX, mx.GetX()); bMaxY = std::max(bMaxY, mx.GetY()); bMaxZ = std::max(bMaxZ, mx.GetZ());
    }
    Vector3 center((bMinX + bMaxX) * 0.5f, (bMinY + bMaxY) * 0.5f, (bMinZ + bMaxZ) * 0.5f);
    float extentX = (bMaxX - bMinX), extentY = (bMaxY - bMinY), extentZ = (bMaxZ - bMinZ);
    float halfSize = std::max({ extentX, extentY, extentZ }) * 0.5f;
    halfSize = std::max(halfSize, 0.01f);

    // 斜め上前方からカメラを配置（45° 仰角・45° 右旋回）
    float dist = halfSize * 3.0f;
    Vector3 camDir(0.7071f, 0.4082f, -0.5774f); // 正規化済み 45°/35° 方向
    Vector3 camPos(
        center.GetX() + camDir.GetX() * dist,
        center.GetY() + camDir.GetY() * dist,
        center.GetZ() + camDir.GetZ() * dist
    );
    camera_.SetPosition(camPos);
    Matrix4x4 viewMat = Matrix4x4::LookAtLH(camPos, center, Vector3::UnitY());
    Quaternion rot = Quaternion::FromRotationMatrix(viewMat.Inverse());
    camera_.SetRotation(rot);

    float farClip = std::max(dist * 4.0f, 100.0f);
    camera_.SetPerspective(3.14159265f / 4.0f, 1.0f, 0.001f, farClip);

    // RenderItem 群を構築
    std::vector<RenderItem> items;
    items.reserve(meshes.size());
    Matrix4x4 identity = Matrix4x4::Identity();
    for (auto* mesh : meshes) {
        if (!mesh) continue;
        items.emplace_back(mesh, mesh->GetMaterial(), identity);
    }

    // サムネイル描画
    RenderView view;
    view.camera    = &camera_;
    view.viewName  = "Thumbnail";
    view.layerMask = 0xFFFFFFFF;

    renderer->DrawToTexture(
        entry.rt->GetResource(),
        entry.rt->GetRTVHandle(),
        entry.rt->GetDSVHandle(),
        view,
        items,
        lights,
        {},    // skinned items なし
        false  // debug draw なし
    );

    entry.ready = true;
}

void ThumbnailRenderer::PreLoadAllAsync(std::atomic<int>& loadedCount) {
    // 別スレッドから呼び出し可能: 全pendingモデルをResourceLoaderにキャッシュ
    auto paths = GetPendingPaths();
    for (const auto& path : paths) {
        ResourceLoader::LoadModel(path);
        loadedCount.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace UnoEngine
