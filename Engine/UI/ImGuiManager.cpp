#include "pch.h"
#include "ImGuiManager.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Window/Window.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#ifdef IMGUI_ENABLE_FREETYPE
#include <imgui_freetype.h>
#endif

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // フォント設定（絵文字対応）
    // 日本語フォントをロード
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\meiryo.ttc", 16.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

#ifdef IMGUI_ENABLE_FREETYPE
    // FreeType使用時は絵文字フォントをマージ
    ImFontConfig emojiConfig;
    emojiConfig.MergeMode = true;
    emojiConfig.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LoadColor;

    // 絵文字のグリフ範囲を設定（Unicodeの絵文字範囲）
    static const ImWchar emojiRanges[] = {
        0x2600, 0x26FF,   // Miscellaneous Symbols
        0x2700, 0x27BF,   // Dingbats
        0xFE00, 0xFE0F,   // Variation Selectors
        0x1F300, 0x1F5FF, // Miscellaneous Symbols and Pictographs
        0x1F600, 0x1F64F, // Emoticons
        0x1F680, 0x1F6FF, // Transport and Map Symbols
        0x1F900, 0x1F9FF, // Supplemental Symbols and Pictographs
        0x1FA00, 0x1FA6F, // Chess Symbols
        0x1FA70, 0x1FAFF, // Symbols and Pictographs Extended-A
        0,
    };

    // Windows 10以降のSegoe UI Emojiを使用
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 16.0f, &emojiConfig, emojiRanges);
#endif

    io.Fonts->Build();

    // ImGuiスタイル設定（カスタムダークテーマ）
    ImGui::StyleColorsDark();
    
    // カスタムダークテーマの適用
    ImGuiStyle& style = ImGui::GetStyle();
    
    // ウィンドウの角を丸く
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 4.0f;
    
    // パディングとスペーシング
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(5.0f, 3.0f);
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    
    // ボーダーサイズ
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    
    // カラーパレット（黒＋赤アクセントテーマ）
    ImVec4* colors = style.Colors;

    // 赤アクセントカラー定義
    ImVec4 accentRed = ImVec4(0.80f, 0.20f, 0.20f, 1.0f);        // メイン赤
    ImVec4 accentRedHover = ImVec4(0.90f, 0.30f, 0.30f, 1.0f);   // ホバー時の赤
    ImVec4 accentRedActive = ImVec4(0.70f, 0.15f, 0.15f, 1.0f);  // アクティブ時の赤
    ImVec4 accentRedDark = ImVec4(0.50f, 0.12f, 0.12f, 1.0f);    // 暗めの赤

    // 背景色（ほぼ黒）
    colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.08f, 0.08f, 0.98f);

    // タイトルバー
    colors[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.04f, 0.04f, 0.04f, 0.75f);

    // メニューバー
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);

    // ヘッダー（コラプシングヘッダー等）- 赤アクセント
    colors[ImGuiCol_Header]               = accentRedDark;
    colors[ImGuiCol_HeaderHovered]        = accentRed;
    colors[ImGuiCol_HeaderActive]         = accentRedActive;

    // タブ - 赤アクセント
    colors[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
    colors[ImGuiCol_TabHovered]           = accentRed;
    colors[ImGuiCol_TabActive]            = accentRedDark;
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.12f, 0.08f, 0.08f, 1.0f);

    // フレーム（入力フィールド等）
    colors[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.15f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.12f, 0.12f, 1.0f);

    // ボタン - 赤アクセント
    colors[ImGuiCol_Button]               = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_ButtonHovered]        = accentRed;
    colors[ImGuiCol_ButtonActive]         = accentRedActive;

    // スクロールバー
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive]  = accentRed;

    // チェックマーク - 赤
    colors[ImGuiCol_CheckMark]            = accentRed;

    // スライダー - 赤
    colors[ImGuiCol_SliderGrab]           = accentRed;
    colors[ImGuiCol_SliderGrabActive]     = accentRedHover;

    // セパレーター
    colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_SeparatorHovered]     = accentRed;
    colors[ImGuiCol_SeparatorActive]      = accentRedHover;

    // リサイズグリップ - 赤
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.15f, 0.15f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]    = accentRed;
    colors[ImGuiCol_ResizeGripActive]     = accentRedHover;

    // ドッキング - 赤
    colors[ImGuiCol_DockingPreview]       = ImVec4(0.80f, 0.20f, 0.20f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]       = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);

    // テキスト
    colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

    // ボーダー - 少し赤みを帯びた暗い色
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // 選択 - 赤
    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.60f, 0.15f, 0.15f, 0.50f);

    // ナビゲーション - 赤
    colors[ImGuiCol_NavHighlight]         = accentRed;

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
