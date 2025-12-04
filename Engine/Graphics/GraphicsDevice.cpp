#include "pch.h"
#include "GraphicsDevice.h"
#include "../Core/Logger.h"
#include <stdexcept>

namespace UnoEngine {

GraphicsDevice::GraphicsDevice(const GraphicsConfig& config)
    : config_(config) {
}

void GraphicsDevice::Initialize(Window* window) {
    if (config_.enableDebugLayer) {
        EnableDebugLayer();
    }

    CreateDevice();
    CreateCommandQueue();
    CreateSwapChain(window);
    CreateRenderTargets();
    CreateDepthStencil();
    CreateSRVHeap();
    CreateFence();

    // コマンドアロケータとリスト作成
    for (uint32 i = 0; i < BACK_BUFFER_COUNT; ++i) {
        ThrowIfFailed(
            device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&commandAllocators_[i])
            ),
            "Failed to create command allocator"
        );
    }

    // リソースアップロード専用のコマンドアロケータ
    ThrowIfFailed(
        device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&uploadCommandAllocator_)
        ),
        "Failed to create upload command allocator"
    );

    ThrowIfFailed(
        device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocators_[0].Get(),
            nullptr,
            IID_PPV_ARGS(&commandList_)
        ),
        "Failed to create command list"
    );

    // 初期状態はクローズ
    commandList_->Close();
}

void GraphicsDevice::EnableDebugLayer() {
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();

        if (config_.enableGPUValidation) {
            ComPtr<ID3D12Debug1> debugController1;
            if (SUCCEEDED(debugController.As(&debugController1))) {
                debugController1->SetEnableGPUBasedValidation(TRUE);
            }
        }
    }
#endif
}

void GraphicsDevice::CreateDevice() {
    // DXGIファクトリ作成
    ThrowIfFailed(
        CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_)),
        "Failed to create DXGI factory"
    );

    // アダプタ選択（最初の利用可能なハードウェアアダプタ）
    ComPtr<IDXGIAdapter1> adapter;
    for (uint32 i = 0; factory_->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // ソフトウェアアダプタをスキップ
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // デバイス作成試行
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_)))) {
            break;
        }
    }

    if (!device_) {
        throw std::runtime_error("Failed to create D3D12 device");
    }
}

void GraphicsDevice::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    ThrowIfFailed(
        device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)),
        "Failed to create command queue"
    );
}

void GraphicsDevice::CreateSwapChain(Window* window) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = window->GetWidth();
    swapChainDesc.Height = window->GetHeight();
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = BACK_BUFFER_COUNT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(
        factory_->CreateSwapChainForHwnd(
            commandQueue_.Get(),
            window->GetHandle(),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        ),
        "Failed to create swap chain"
    );

    ThrowIfFailed(
        swapChain1.As(&swapChain_),
        "Failed to query IDXGISwapChain3"
    );

    // Alt+Enterでのフルスクリーン切替を無効化
    factory_->MakeWindowAssociation(window->GetHandle(), DXGI_MWA_NO_ALT_ENTER);

    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

void GraphicsDevice::CreateRenderTargets() {
    // RTVヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = BACK_BUFFER_COUNT;

    ThrowIfFailed(
        device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)),
        "Failed to create RTV heap"
    );

    rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // バックバッファ用RTV作成
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (uint32 i = 0; i < BACK_BUFFER_COUNT; ++i) {
        ThrowIfFailed(
            swapChain_->GetBuffer(i, IID_PPV_ARGS(&renderTargets_[i])),
            "Failed to get swap chain buffer"
        );

        // sRGBフォーマットでRTVを作成（ガンマ補正用）
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        
        device_->CreateRenderTargetView(renderTargets_[i].Get(), &rtvDesc, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize_;
    }
}

void GraphicsDevice::CreateDepthStencil() {
    // DSVヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;

    ThrowIfFailed(
        device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_)),
        "Failed to create DSV heap"
    );

    // 深度ステンシルバッファ作成
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChain_->GetDesc(&swapChainDesc);

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = swapChainDesc.BufferDesc.Width;
    depthDesc.Height = swapChainDesc.BufferDesc.Height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(
        device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&depthStencil_)
        ),
        "Failed to create depth stencil buffer"
    );

    // DSV作成
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    device_->CreateDepthStencilView(depthStencil_.Get(), &dsvDesc,
                                    dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}


void GraphicsDevice::CreateSRVHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = MAX_SRV_COUNT;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ThrowIfFailed(
        device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_)),
        "Failed to create SRV heap"
    );

    srvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    );
}

void GraphicsDevice::CreateSRV(ID3D12Resource* resource, uint32 index) {
    if (index >= MAX_SRV_COUNT) {
        throw std::runtime_error("SRV index out of range");
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    DXGI_FORMAT format = resource->GetDesc().Format;
    
    // リソースフォーマットをそのまま使用（既にsRGBの場合はsRGBで扱われる）
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = resource->GetDesc().MipLevels;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * srvDescriptorSize_;

    device_->CreateShaderResourceView(resource, &srvDesc, handle);
}

uint32 GraphicsDevice::AllocateSRVIndex() {
    if (nextSRVIndex_ >= MAX_SRV_COUNT - 100) {  // 100個は予約領域（ボーン行列等）
        throw std::runtime_error("SRV heap exhausted");
    }
    return nextSRVIndex_++;
}

uint32 GraphicsDevice::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const {
    return device_->GetDescriptorHandleIncrementSize(type);
}

void GraphicsDevice::CreateFence() {
    ThrowIfFailed(
        device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
        "Failed to create fence"
    );

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        throw std::runtime_error("Failed to create fence event");
    }
}

void GraphicsDevice::BeginFrame() {
    // このバックバッファのGPU処理完了を待つ
    if (fence_->GetCompletedValue() < fenceValues_[currentBackBufferIndex_]) {
        fence_->SetEventOnCompletion(fenceValues_[currentBackBufferIndex_], fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // コマンドアロケータをリセット
    ThrowIfFailed(
        commandAllocators_[currentBackBufferIndex_]->Reset(),
        "Failed to reset command allocator"
    );

    // コマンドリストをリセット
    ThrowIfFailed(
        commandList_->Reset(commandAllocators_[currentBackBufferIndex_].Get(), nullptr),
        "Failed to reset command list"
    );

    // レンダーターゲットへの遷移
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets_[currentBackBufferIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList_->ResourceBarrier(1, &barrier);

    // レンダーターゲットをクリア
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += currentBackBufferIndex_ * rtvDescriptorSize_;

    const float clearColor[] = { 0.2f, 0.3f, 0.4f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 深度ステンシルをクリア
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // レンダーターゲットと深度ステンシルを設定
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
}

void GraphicsDevice::SetBackBufferAsRenderTarget() {
    auto* cmdList = commandList_.Get();
    
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += currentBackBufferIndex_ * rtvDescriptorSize_;
    
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
}

void GraphicsDevice::EndFrame() {
    // プレゼントへの遷移
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets_[currentBackBufferIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList_->ResourceBarrier(1, &barrier);

    // コマンドリストをクローズ
    ThrowIfFailed(
        commandList_->Close(),
        "Failed to close command list"
    );

    // コマンドキューに投入
    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, cmdLists);
}

void GraphicsDevice::Present() {
    // 画面更新
    ThrowIfFailed(
        swapChain_->Present(1, 0),
        "Failed to present"
    );

    // 現在のフレームにフェンス値を設定
    const uint64 currentFence = ++currentFenceValue_;
    commandQueue_->Signal(fence_.Get(), currentFence);
    fenceValues_[currentBackBufferIndex_] = currentFence;

    // 次のバックバッファインデックスを取得
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

void GraphicsDevice::WaitForGPU() {
    const uint64 fenceValue = ++currentFenceValue_;
    commandQueue_->Signal(fence_.Get(), fenceValue);
    fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
    WaitForSingleObject(fenceEvent_, INFINITE);
}

void GraphicsDevice::BeginResourceUpload() {
    Logger::Debug("[GraphicsDevice] BeginResourceUpload: GPUを同期中...");

    // GPUが前回のコマンドを完了するまで待つ
    WaitForGPU();

    Logger::Debug("[GraphicsDevice] BeginResourceUpload: アップロード用コマンドアロケータをリセット中...");

    // アップロード専用コマンドアロケータをリセット
    ThrowIfFailed(
        uploadCommandAllocator_->Reset(),
        "Failed to reset upload command allocator"
    );

    Logger::Debug("[GraphicsDevice] BeginResourceUpload: コマンドリストをリセット中...");

    // コマンドリストをリセット（開く）
    // Note: Resetが失敗する場合、コマンドリストが既に開いている
    HRESULT hr = commandList_->Reset(uploadCommandAllocator_.Get(), nullptr);
    if (FAILED(hr)) {
        Logger::Warning("[GraphicsDevice] コマンドリストが既に開いています。クローズしてリトライします。");
        // コマンドリストが既に開いている場合、先にクローズしてからリセット
        commandList_->Close();
        WaitForGPU();
        uploadCommandAllocator_->Reset();
        hr = commandList_->Reset(uploadCommandAllocator_.Get(), nullptr);
        ThrowIfFailed(hr, "Failed to reset command list for resource upload after close");
    }

    Logger::Debug("[GraphicsDevice] BeginResourceUpload: 完了");
}

void GraphicsDevice::EndResourceUpload() {
    Logger::Debug("[GraphicsDevice] EndResourceUpload: コマンドリストをクローズ中...");

    // コマンドリストをクローズ
    ThrowIfFailed(
        commandList_->Close(),
        "Failed to close command list after resource upload"
    );

    Logger::Debug("[GraphicsDevice] EndResourceUpload: コマンドキューに投入中...");

    // コマンドキューに投入
    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, cmdLists);

    Logger::Debug("[GraphicsDevice] EndResourceUpload: GPUの完了を待機中...");

    // GPU完了を待つ
    WaitForGPU();

    Logger::Debug("[GraphicsDevice] EndResourceUpload: 完了");
}

void GraphicsDevice::OnResize(uint32 width, uint32 height) {
    if (width == 0 || height == 0) return;

    // GPU処理が完了するまで待つ
    WaitForGPU();

    // 既存のリソースを解放
    for (uint32 i = 0; i < BACK_BUFFER_COUNT; ++i) {
        renderTargets_[i].Reset();
    }
    depthStencil_.Reset();

    // スワップチェーンのリサイズ
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChain_->GetDesc(&swapChainDesc);
    ThrowIfFailed(
        swapChain_->ResizeBuffers(
            BACK_BUFFER_COUNT,
            width,
            height,
            swapChainDesc.BufferDesc.Format,
            swapChainDesc.Flags
        ),
        "Failed to resize swap chain buffers"
    );

    // レンダーターゲットと深度バッファを再作成
    CreateRenderTargets();
    CreateDepthStencil();

    // 次のフレームのインデックスを取得
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

} // namespace UnoEngine
