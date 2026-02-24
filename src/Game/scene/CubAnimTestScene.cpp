#include "CubAnimTestScene.h"
#include "SceneManager.h"
#include "UnoEngine.h"
#include <wincodec.h>
#include <filesystem>
#pragma comment(lib, "windowscodecs.lib")

#ifdef _DEBUG
#include "imgui.h"
#endif

// PW_RENDERFULLCONTENT: Windows 8.1+ で DX/GPU コンテンツをキャプチャ
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

// ─────────────────────────────────────────────
// スクリーンショット保存（WIC で PNG）
// ─────────────────────────────────────────────
static void TakeScreenshot(HWND hwnd) {
    // 1. ウィンドウのクライアントサイズを取得
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int w = rc.right;
    const int h = rc.bottom;
    if (w <= 0 || h <= 0) return;

    // 2. メモリDCと32bit DIBを作成
    HDC hdcWin = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWin);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;   // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP hbm    = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    // 3. PrintWindow で DX12 コンテンツをキャプチャ
    PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT | PW_CLIENTONLY);

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWin);

    // 4. 出力先フォルダを作成
    std::filesystem::create_directories("screenshots");

    // 5. タイムスタンプ付きファイル名
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t filename[256];
    swprintf_s(filename, L"screenshots\\screenshot_%04d%02d%02d_%02d%02d%02d.png",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    // 6. WIC で PNG 保存
    IWICImagingFactory* factory = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {

        IWICBitmap* wicBmp = nullptr;
        if (SUCCEEDED(factory->CreateBitmapFromHBITMAP(
            hbm, nullptr, WICBitmapIgnoreAlpha, &wicBmp))) {

            IWICStream* wicStream = nullptr;
            factory->CreateStream(&wicStream);
            if (wicStream && SUCCEEDED(
                wicStream->InitializeFromFilename(filename, GENERIC_WRITE))) {

                IWICBitmapEncoder* encoder = nullptr;
                factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
                if (encoder && SUCCEEDED(
                    encoder->Initialize(wicStream, WICBitmapEncoderNoCache))) {

                    IWICBitmapFrameEncode* frame = nullptr;
                    IPropertyBag2* props = nullptr;
                    encoder->CreateNewFrame(&frame, &props);
                    if (frame && SUCCEEDED(frame->Initialize(props))) {
                        UINT bw, bh;
                        wicBmp->GetSize(&bw, &bh);
                        frame->SetSize(bw, bh);
                        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
                        frame->SetPixelFormat(&fmt);
                        frame->WriteSource(wicBmp, nullptr);
                        frame->Commit();
                        encoder->Commit();

                        OutputDebugStringW((std::wstring(L"[Screenshot] Saved: ") + filename + L"\n").c_str());
                    }
                    if (props) props->Release();
                    if (frame)  frame->Release();
                }
                if (encoder) encoder->Release();
            }
            if (wicStream) wicStream->Release();
            wicBmp->Release();
        }
        factory->Release();
    }

    DeleteObject(hbm);
}

// ─────────────────────────────────────────────
// シーン実装
// ─────────────────────────────────────────────
void CubAnimTestScene::Initialize() {
    UnoEngine* engine = UnoEngine::GetInstance();

    camera_->SetTranslate({0.0f, 2.0f, -6.0f});
    camera_->SetRotate({0.1f, 0.0f, 0.0f});

    model_ = engine->CreateAnim();
    model_->LoadFromFile("Resources/Models/cub", "cub_walk.gltf");

    Animation walkAnim = model_->GetAnimationPlayer().GetAnimation();
    model_->AddAnimation("Walk", walkAnim);

    Animation runAnim = engine->LoadAnim("Resources/Models/cub", "cub_run.gltf");
    model_->AddAnimation("Run", runAnim);

    model_->ChangeAnimation("Walk");
    model_->PlayAnimation();

    object_ = engine->CreateObj3();
    object_->SetModel(static_cast<Model*>(model_.get()));
    object_->SetAnimatedModel(model_.get());
    object_->SetPosition({0.0f, 0.0f, 0.0f});
    object_->SetScale({modelScale_, modelScale_, modelScale_});
    object_->SetRotation({0.0f, 3.14159f, 0.0f});
    object_->SetEnableLighting(true);
    object_->SetCamera(camera_);
    object_->Update();
}

void CubAnimTestScene::Update() {
    UnoEngine* engine = UnoEngine::GetInstance();
    const float dt = engine->GetDelta();
    elapsedTime_ += dt;

    // ── Escape: アプリ終了 ──
    if (engine->IsKeyTrig(DIK_ESCAPE)) {
        PostQuitMessage(0);
        return;
    }

    // ── F1: スクリーンショット ──
    if (engine->IsKeyTrig(DIK_F1)) {
        HWND hwnd = engine->GetWinApp()->GetHwnd();
        TakeScreenshot(hwnd);
    }

    // ── WASD + QE: カメラ移動 ──
    Input* input = engine->GetInput();
    const float spd = cameraSpeed_ * dt;
    if (input->PushKey(DIK_W)) camera_->MoveForward( spd);
    if (input->PushKey(DIK_S)) camera_->MoveForward(-spd);
    if (input->PushKey(DIK_A)) camera_->MoveRight(-spd);
    if (input->PushKey(DIK_D)) camera_->MoveRight( spd);
    if (input->PushKey(DIK_Q)) camera_->MoveUp(-spd);
    if (input->PushKey(DIK_E)) camera_->MoveUp( spd);

    // ── 右クリックホールド: マウス視点 ──
    DIMOUSESTATE ms{};
    if (SUCCEEDED(input->GetMouseState(&ms)) && (ms.rgbButtons[1] & 0x80)) {
        float dx, dy;
        input->GetMouseMovement(dx, dy);
        camera_->ProcessMouseInput(dx, dy);
    }

    // ── アニメーション・オブジェクト更新 ──
    model_->Update(dt);
    object_->Update();
    camera_->Update();

#ifdef _DEBUG
    ImGui::Begin("Cub Animation Test");

    // スケール
    ImGui::SeparatorText("Scale");
    if (ImGui::SliderFloat("Scale", &modelScale_, 0.001f, 5.0f)) {
        object_->SetScale({modelScale_, modelScale_, modelScale_});
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##scale")) {
        modelScale_ = 1.0f;
        object_->SetScale({1.0f, 1.0f, 1.0f});
    }

    // カメラ
    ImGui::SeparatorText("Camera  [WASD/QE move | RMB drag = look]");
    ImGui::SliderFloat("Speed", &cameraSpeed_, 0.5f, 30.0f);
    Vector3 pos = camera_->GetTranslate();
    ImGui::Text("Pos: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);

    // アニメーション
    ImGui::SeparatorText("Animation");
    ImGui::Text("Current : %s", model_->GetCurrentAnimationName().c_str());
    ImGui::Text("Blending: %s", model_->IsBlending() ? "YES" : "NO");
    ImGui::ProgressBar(model_->GetBlendProgress(), ImVec2(-1.0f, 0.0f));

    ImGui::SliderFloat("Transition sec", &transitionDuration_, 0.05f, 1.0f);

    if (ImGui::Button("Walk (instant)"))  model_->ChangeAnimation("Walk");
    ImGui::SameLine();
    if (ImGui::Button("Run  (instant)"))  model_->ChangeAnimation("Run");

    if (ImGui::Button("Walk (blend)"))    model_->TransitionToAnimation("Walk", transitionDuration_);
    ImGui::SameLine();
    if (ImGui::Button("Run  (blend)"))    model_->TransitionToAnimation("Run",  transitionDuration_);

    // スクリーンショット
    ImGui::SeparatorText("Screenshot  [F1]");
    if (ImGui::Button("Capture PNG")) {
        HWND hwnd = engine->GetWinApp()->GetHwnd();
        TakeScreenshot(hwnd);
    }
    ImGui::TextDisabled("Saved to: screenshots/screenshot_YYYYMMDD_HHMMSS.png");

    ImGui::End();
#endif
}

void CubAnimTestScene::Draw() {
    object_->Draw();
}

void CubAnimTestScene::Finalize() {
    object_.reset();
    model_.reset();
}
