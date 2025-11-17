#include "ImGuiManager.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Window/Window.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

namespace UnoEngine {

ImGuiManager::~ImGuiManager() {
    Shutdown();
}

void ImGuiManager::Initialize(GraphicsDevice* graphics, Window* window, uint32 srvDescriptorIndex) {
    graphics_ = graphics;
    srvDescriptorIndex_ = srvDescriptorIndex;

    // ImGuiコンテキスト作成
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // キーボードナビゲーション有効化

    // ImGuiスタイル設定
    ImGui::StyleColorsDark();

    // Win32バックエンド初期化
    ImGui_ImplWin32_Init(window->GetHandle());

    // DX12バックエンド初期化
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = graphics->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = graphics->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();

    UINT descriptorSize = graphics->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuHandle.ptr += srvDescriptorIndex * descriptorSize;
    gpuHandle.ptr += srvDescriptorIndex * descriptorSize;

    ImGui_ImplDX12_Init(
        graphics->GetDevice(),
        3, // バックバッファ数
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        graphics->GetSRVHeap(),
        cpuHandle,
        gpuHandle
    );

    initialized_ = true;
}

void ImGuiManager::Shutdown() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
}

void ImGuiManager::BeginFrame() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::EndFrame() {
    if (!initialized_) {
        return;
    }

    ImGui::Render();
}

void ImGuiManager::Render(ID3D12GraphicsCommandList* commandList) {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

bool ImGuiManager::WantCaptureMouse() const {
    if (!initialized_) {
        return false;
    }
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiManager::WantCaptureKeyboard() const {
    if (!initialized_) {
        return false;
    }
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace UnoEngine
