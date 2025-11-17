#pragma once

#include "D3D12Common.h"
#include "../Core/NonCopyable.h"
#include "../Window/Window.h"

namespace UnoEngine {

// グラフィックスデバイス初期化設定
struct GraphicsConfig {
    bool enableDebugLayer = false;
    bool enableGPUValidation = false;
};

// DirectX12デバイス管理
class GraphicsDevice : public NonCopyable {
public:
    explicit GraphicsDevice(const GraphicsConfig& config = {});
    ~GraphicsDevice() = default;

    // 初期化
    void Initialize(Window* window);

    // フレーム処理
    void BeginFrame();
    void EndFrame();
    void Present();

    // アクセサ
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    IDXGISwapChain3* GetSwapChain() const { return swapChain_.Get(); }
    uint32 GetCurrentBackBufferIndex() const { return currentBackBufferIndex_; }
    ID3D12DescriptorHeap* GetDSVHeap() const { return dsvHeap_.Get(); }

private:
    void EnableDebugLayer();
    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain(Window* window);
    void CreateRenderTargets();
    void CreateDepthStencil();
    void CreateFence();
    void WaitForGPU();

private:
    // 設定
    GraphicsConfig config_;

    // DirectX12コアオブジェクト
    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> commandQueue_;
    ComPtr<IDXGISwapChain3> swapChain_;

    // コマンドリスト
    ComPtr<ID3D12CommandAllocator> commandAllocators_[BACK_BUFFER_COUNT];
    ComPtr<ID3D12GraphicsCommandList> commandList_;

    // レンダーターゲット
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12Resource> renderTargets_[BACK_BUFFER_COUNT];
    uint32 rtvDescriptorSize_ = 0;

    // 深度ステンシル
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    ComPtr<ID3D12Resource> depthStencil_;

    // 同期オブジェクト
    ComPtr<ID3D12Fence> fence_;
    uint64 fenceValues_[BACK_BUFFER_COUNT] = {};
    uint64 currentFenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // 状態
    uint32 currentBackBufferIndex_ = 0;
};

} // namespace UnoEngine
