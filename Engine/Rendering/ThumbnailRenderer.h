#pragma once

#include "../Graphics/RenderTexture.h"
#include "../Core/Camera.h"
#include <unordered_map>
#include <deque>
#include <string>
#include <memory>
#include <atomic>
#include <vector>

namespace UnoEngine {

class Renderer;
class LightManager;
class GraphicsDevice;
class ResourceManager;

class ThumbnailRenderer {
public:
    ThumbnailRenderer()  = default;
    ~ThumbnailRenderer() = default;

    void Initialize(GraphicsDevice* graphics, ResourceManager* resources);

    // モデルパスのサムネイルを要求。準備完了なら SRV ハンドル、未準備なら ptr=0 を返す。
    D3D12_GPU_DESCRIPTOR_HANDLE Request(const std::string& modelPath);
    bool IsReady(const std::string& modelPath) const;
    bool HasPending() const { return !pending_.empty(); }
    bool IsInitialized() const { return initialized_; }
    size_t GetPendingCount() const { return pending_.size(); }
    size_t GetTotalCount() const { return cache_.size(); }

    // Phase 1: BeginFrame前にモデルをキャッシュへロード（コマンドリストが閉じている状態）
    void PreLoadPending();
    // Phase 2: BeginFrame後にサムネイルを描画（コマンドリストがオープンな状態）
    void ProcessOne(Renderer* renderer, LightManager* lights, GraphicsDevice* graphics);

    // バックグラウンドで全モデルを一括プリロード（別スレッドから呼び出し可能）
    void PreLoadAllAsync(std::atomic<int>& loadedCount);
    // プリロード対象パスリストを取得
    std::vector<std::string> GetPendingPaths() const { return { pending_.begin(), pending_.end() }; }

    static constexpr uint32_t kSize = 128;

private:
    struct Entry {
        std::unique_ptr<RenderTexture> rt;
        bool ready = false;
    };

    std::unordered_map<std::string, Entry> cache_;
    std::deque<std::string> pending_;
    Camera camera_;
    GraphicsDevice* graphics_ = nullptr;
    bool initialized_ = false;
};

} // namespace UnoEngine
