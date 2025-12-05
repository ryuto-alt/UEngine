// UTF-8 BOM: This file should be saved with UTF-8 encoding
#include "pch.h"
#include "ParticleEditor.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/RenderTexture.h"
#include "../Core/Camera.h"
#include "../Graphics/d3dx12.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace UnoEngine {

// 3D点をスクリーン座標に変換する構造体（グリッドとパーティクル描画で共有）
struct Transform3D {
    float centerX, centerY;
    float scale;
    float camAngle, camPitch;
    float orbitDistance;

    // 3D座標をスクリーン座標に変換
    ImVec2 project(float x, float y, float z) const {
        float cosCam = std::cos(camAngle);
        float sinCam = std::sin(camAngle);
        float cosPitch = std::cos(camPitch);
        float sinPitch = std::sin(camPitch);

        // カメラ回転適用
        float viewX = x * cosCam - z * sinCam;
        float viewZ = x * sinCam + z * cosCam;
        
        // ピッチ回転（正のピッチで上から見下ろす）
        float viewY = y * cosPitch + viewZ * sinPitch;
        float finalZ = -y * sinPitch + viewZ * cosPitch;

        // 透視投影
        float depth = finalZ + orbitDistance;
        if (depth < 0.5f) depth = 0.5f;
        float depthScale = orbitDistance / depth;

        return ImVec2(
            centerX + viewX * scale * depthScale,
            centerY - viewY * scale * depthScale
        );
    }

    // 深度値を取得
    float getDepth(float x, float y, float z) const {
        float cosCam = std::cos(camAngle);
        float sinCam = std::sin(camAngle);
        float cosPitch = std::cos(camPitch);
        float sinPitch = std::sin(camPitch);

        float viewZ = x * sinCam + z * cosCam;
        return -y * sinPitch + viewZ * cosPitch;
    }
};;

ParticleEditor::ParticleEditor() = default;
ParticleEditor::~ParticleEditor() = default;

void ParticleEditor::Initialize(GraphicsDevice* graphics, ParticleSystem* particleSystem) {
    graphics_ = graphics;
    particleSystem_ = particleSystem;

    // プレビューカメラ作成
    previewCamera_ = std::make_unique<Camera>();
    float aspectRatio = static_cast<float>(previewWidth_) / static_cast<float>(previewHeight_);
    previewCamera_->SetPerspective(45.0f * 3.14159f / 180.0f, aspectRatio, 0.1f, 1000.0f);
    
    // 初期カメラ位置をオービットパラメータから計算
    UpdatePreviewCamera();

    // プレビュー用レンダーテクスチャ作成
    previewRenderTexture_ = std::make_unique<RenderTexture>();
    uint32 srvIndex = graphics_->AllocateSRVIndex();
    previewRenderTexture_->Create(graphics_, previewWidth_, previewHeight_, srvIndex);
}

void ParticleEditor::Draw() {
    if (!isVisible_) return;

    ImGui::SetNextWindowSize(ImVec2(1200, 900), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("パーティクルエディター###ParticleEditor", &isVisible_, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    DrawMenuBar();
    DrawToolbar();

    // 左右分割レイアウト
    ImGui::Columns(2, "particle_editor_columns");

    // 左側: エミッターリスト + プロパティ
    DrawEmitterList();
    DrawEmitterProperties();

    ImGui::NextColumn();

    // 右側: プレビュー
    if (showPreview_) {
        DrawPreviewWindow();
    }

    ImGui::Columns(1);

    ImGui::End();
}

void ParticleEditor::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("ファイル")) {
            if (ImGui::MenuItem("新規作成", "Ctrl+N")) {
                NewEffect();
            }
            if (ImGui::MenuItem("開く...", "Ctrl+O")) {
                OpenEffect();
            }
            if (ImGui::MenuItem("保存", "Ctrl+S", false, !currentFilePath_.empty())) {
                SaveEffect();
            }
            if (ImGui::MenuItem("名前を付けて保存...")) {
                SaveEffectAs();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("閉じる")) {
                isVisible_ = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("編集")) {
            if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, false)) {
                // TODO: Undo
            }
            if (ImGui::MenuItem("やり直す", "Ctrl+Y", false, false)) {
                // TODO: Redo
            }
            ImGui::Separator();
            if (ImGui::MenuItem("すべてクリア")) {
                if (particleSystem_) {
                    particleSystem_->RemoveAllEmitters();
                    selectedEmitter_ = nullptr;
                    selectedEmitterIndex_ = -1;
                    hasUnsavedChanges_ = true;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("表示")) {
            ImGui::MenuItem("プレビュー", nullptr, &showPreview_);
            ImGui::MenuItem("3Dプレビュー", nullptr, &use3DPreview_);
            ImGui::Separator();
            ImGui::MenuItem("グリッド表示", nullptr, &showGrid_);
            ImGui::MenuItem("軸表示", nullptr, &showAxis_);
            ImGui::Separator();
            ImGui::MenuItem("自動回転", nullptr, &autoRotatePreview_);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("プリセット")) {
            ImGui::SeparatorText("自然現象");
            if (ImGui::MenuItem("炎エフェクト")) {
                CreateDemoPreset();
            }
            if (ImGui::MenuItem("煙エフェクト")) {
                CreateSmokePreset();
            }
            if (ImGui::MenuItem("火花エフェクト")) {
                CreateSparkPreset();
            }
            
            ImGui::SeparatorText("魔法・ファンタジー");
            if (ImGui::MenuItem("オーラエフェクト")) {
                CreateAuraPreset();
            }
            if (ImGui::MenuItem("爆発エフェクト")) {
                CreateExplosionPreset();
            }
            
            ImGui::SeparatorText("環境");
            if (ImGui::MenuItem("雨エフェクト")) {
                CreateRainPreset();
            }
            if (ImGui::MenuItem("雪エフェクト")) {
                CreateSnowPreset();
            }
            
            ImGui::SeparatorText("高度な3Dエフェクト");
            if (ImGui::MenuItem("竜巻エフェクト")) {
                CreateTornadoPreset();
            }
            if (ImGui::MenuItem("渦巻きエフェクト")) {
                CreateVortexPreset();
            }
            if (ImGui::MenuItem("魔法陣エフェクト")) {
                CreateMagicCirclePreset();
            }
            if (ImGui::MenuItem("剣の軌跡エフェクト")) {
                CreateBladeTrailPreset();
            }
            ImGui::EndMenu();
        }

        // ヘルプ
        if (ImGui::BeginMenu("ヘルプ")) {
            if (ImGui::MenuItem("操作方法")) {
                ImGui::OpenPopup("HelpPopup");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
    
    // ヘルプポップアップ
    if (ImGui::BeginPopupModal("HelpPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("パーティクルエディター操作方法");
        ImGui::Separator();
        ImGui::BulletText("プレビュー操作:");
        ImGui::Indent();
        ImGui::BulletText("左ドラッグ: カメラ回転");
        ImGui::BulletText("右ドラッグ: カメラパン");
        ImGui::BulletText("ホイール: ズーム");
        ImGui::Unindent();
        ImGui::Spacing();
        ImGui::BulletText("エミッター:");
        ImGui::Indent();
        ImGui::BulletText("追加: 「+エミッター追加」ボタン");
        ImGui::BulletText("選択: リストからクリック");
        ImGui::BulletText("削除: 「削除」ボタン");
        ImGui::Unindent();
        ImGui::Spacing();
        if (ImGui::Button("閉じる", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ParticleEditor::DrawToolbar() {
    // 再生コントロール（アイコン風）
    ImGui::PushStyleColor(ImGuiCol_Button, isPlaying_ ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    if (isPlaying_) {
        if (ImGui::Button(" || ", ImVec2(40, 0))) {  // 一時停止
            isPlaying_ = false;
            if (particleSystem_) particleSystem_->Pause();
        }
    } else {
        if (ImGui::Button(" > ", ImVec2(40, 0))) {  // 再生
            isPlaying_ = true;
            if (particleSystem_) particleSystem_->Play();
        }
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(isPlaying_ ? "一時停止" : "再生");

    ImGui::SameLine();
    if (ImGui::Button(" [] ", ImVec2(40, 0))) {  // 停止
        isPlaying_ = false;
        if (particleSystem_) particleSystem_->Stop();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("停止");

    ImGui::SameLine();
    if (ImGui::Button(" <| ", ImVec2(40, 0))) {  // リスタート
        if (particleSystem_) particleSystem_->Restart();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("リスタート");

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // 再生速度
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("##Speed", &playbackSpeed_, 0.1f, 3.0f, "%.1fx");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("再生速度");

    // 統計表示
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    
    // プログレスバー風のパーティクル数表示
    if (particleSystem_) {
        uint32 alive = particleSystem_->GetAliveParticleCount();
        uint32 maxP = particleSystem_->GetMaxParticles();
        float ratio = maxP > 0 ? static_cast<float>(alive) / static_cast<float>(maxP) : 0.0f;
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, 
            ratio > 0.9f ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f) :
            ratio > 0.7f ? ImVec4(0.8f, 0.6f, 0.2f, 1.0f) :
                           ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
        
        char overlay[64];
        snprintf(overlay, sizeof(overlay), "%u / %u", alive, maxP);
        ImGui::SetNextItemWidth(150);
        ImGui::ProgressBar(ratio, ImVec2(0, 0), overlay);
        ImGui::PopStyleColor();
        
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("アクティブパーティクル数");
    }

    ImGui::Separator();
}

void ParticleEditor::DrawEmitterList() {
    ImGui::BeginChild("EmitterList", ImVec2(0, 150), true);
    ImGui::Text("エミッター一覧");
    ImGui::Separator();

    if (particleSystem_) {
        for (size_t i = 0; i < particleSystem_->GetEmitterCount(); ++i) {
            auto* emitter = particleSystem_->GetEmitter(i);
            if (emitter) {
                bool isSelected = (selectedEmitterIndex_ == static_cast<int>(i));
                if (ImGui::Selectable(emitter->GetConfig().name.c_str(), isSelected)) {
                    selectedEmitterIndex_ = static_cast<int>(i);
                    selectedEmitter_ = emitter;
                }
            }
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("+ エミッター追加")) {
        if (particleSystem_) {
            auto* newEmitter = particleSystem_->CreateEmitter("New Emitter");
            newEmitter->Play();
            selectedEmitter_ = newEmitter;
            selectedEmitterIndex_ = static_cast<int>(particleSystem_->GetEmitterCount() - 1);
            hasUnsavedChanges_ = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("削除") && selectedEmitter_) {
        particleSystem_->RemoveEmitter(selectedEmitter_);
        selectedEmitter_ = nullptr;
        selectedEmitterIndex_ = 0;
        hasUnsavedChanges_ = true;
    }

    ImGui::EndChild();
}

void ParticleEditor::DrawPreviewWindow() {
    // プレビューウィンドウの高さを動的に計算（利用可能な高さの60%を使用、最小400px）
    float availableHeight = ImGui::GetContentRegionAvail().y;
    float previewHeight = std::max(400.0f, availableHeight * 0.6f);

    ImGui::BeginChild("Preview", ImVec2(0, previewHeight), true);
    
    // ヘッダー行：タイトルとパーティクル数
    uint32 aliveCount = particleSystem_ ? particleSystem_->GetAliveParticleCount() : 0;
    ImGui::Text("3D プレビュー (%u particles)", aliveCount);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    ImGui::Checkbox("グリッド", &showGrid_);
    ImGui::SameLine();
    ImGui::Checkbox("軸", &showAxis_);
    ImGui::Separator();

    // プレビューキャンバスサイズを取得
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y -= 60;  // コントロール用スペース
    
    if (canvasSize.x < 100) canvasSize.x = 100;
    if (canvasSize.y < 100) canvasSize.y = 100;

    // レンダーテクスチャのリサイズ（必要な場合）
    uint32 newWidth = static_cast<uint32>(canvasSize.x);
    uint32 newHeight = static_cast<uint32>(canvasSize.y);
    if (previewRenderTexture_ && (newWidth != previewWidth_ || newHeight != previewHeight_)) {
        if (newWidth > 0 && newHeight > 0) {
            previewWidth_ = newWidth;
            previewHeight_ = newHeight;
            previewRenderTexture_->Resize(graphics_, previewWidth_, previewHeight_);
            UpdatePreviewCamera();
        }
    }

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 3Dプレビュー表示（RenderTextureをImGuiで表示）
    if (use3DPreview_ && previewRenderTexture_) {
        // ImGuiでRenderTextureを表示
        ImGui::Image(
            (ImTextureID)(previewRenderTexture_->GetSRVHandle().ptr),
            canvasSize
        );
        
        // マウス入力処理（Imageの上にInvisibleButtonを重ねてドラッグをキャプチャ）
        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("##PreviewCanvas3D", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            HandlePreviewInput(canvasPos, canvasSize);
        }
    } else {
        // フォールバック：2Dプレビュー
        // 背景
        drawList->AddRectFilledMultiColor(
            canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(20, 20, 30, 255),
            IM_COL32(20, 20, 30, 255),
            IM_COL32(40, 40, 50, 255),
            IM_COL32(40, 40, 50, 255)
        );
        
        // グリッド
        if (showGrid_) {
            DrawGrid(drawList, canvasPos, canvasSize);
        }
        
        // パーティクル描画
        DrawPreviewParticles(drawList, canvasPos, canvasSize);
        
        // 枠線
        drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(80, 80, 90, 255));

        // InvisibleButtonでマウス操作をキャプチャ（ウィンドウドラッグを防ぐ）
        ImGui::InvisibleButton("##PreviewCanvas2D", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        
        // マウス入力処理
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            HandlePreviewInput(canvasPos, canvasSize);
        }
    }

    // カメラコントロール
    ImGui::Spacing();
    ImGui::Text("カメラ操作: 左ドラッグ=回転, Ctrl+左ドラッグ=高さ, 右ドラッグ=パン, ホイール=ズーム");
    
    ImGui::Checkbox("自動回転", &autoRotatePreview_);
    ImGui::SameLine();
    if (ImGui::Button("リセット")) {
        previewOrbitAngle_ = 0.0f;
        previewOrbitPitch_ = 0.785f;  // 45度斜め上から見下ろす
        previewOrbitDistance_ = 12.0f;
        previewOrbitTarget_ = { 0.0f, 1.0f, 0.0f };
        UpdatePreviewCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("上から")) {
        previewOrbitAngle_ = 0.0f;
        previewOrbitPitch_ = 1.5f;  // ほぼ真上から
        previewOrbitDistance_ = 15.0f;
        previewOrbitTarget_ = { 0.0f, 0.0f, 0.0f };
        UpdatePreviewCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("横から")) {
        previewOrbitAngle_ = 0.0f;
        previewOrbitPitch_ = 0.0f;  // 水平
        previewOrbitDistance_ = 12.0f;
        previewOrbitTarget_ = { 0.0f, 1.0f, 0.0f };
        UpdatePreviewCamera();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat("距離", &previewOrbitDistance_, 0.1f, 1.0f, 50.0f)) {
        UpdatePreviewCamera();
    }

    ImGui::EndChild();
}

void ParticleEditor::HandlePreviewInput(const ImVec2& canvasPos, const ImVec2& canvasSize) {
    ImGuiIO& io = ImGui::GetIO();
    
    // マウスホイールでズーム（ホバー時のみ）
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
        previewOrbitDistance_ -= io.MouseWheel * 0.5f;
        previewOrbitDistance_ = (std::max)(1.0f, (std::min)(previewOrbitDistance_, 50.0f));
        UpdatePreviewCamera();
    }
    
    // 左ドラッグ
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (!isOrbitDragging_) {
            isOrbitDragging_ = true;
            lastMousePos_ = io.MousePos;
        }
        
        float deltaX = io.MousePos.x - lastMousePos_.x;
        float deltaY = io.MousePos.y - lastMousePos_.y;
        
        if (io.KeyCtrl) {
            // Ctrl+左ドラッグ: カメラ高さ調整（上ドラッグで下に移動）
            float heightSpeed = previewOrbitDistance_ * 0.005f;
            previewOrbitTarget_.y += deltaY * heightSpeed;
        } else {
            // 通常の左ドラッグ: オービット回転
            previewOrbitAngle_ -= deltaX * 0.01f;
            previewOrbitPitch_ -= deltaY * 0.01f;
            
            // ピッチ制限（真上・真下を防ぐ）
            const float pitchLimit = 1.5f;
            previewOrbitPitch_ = (std::max)(-pitchLimit, (std::min)(previewOrbitPitch_, pitchLimit));
        }
        
        lastMousePos_ = io.MousePos;
        UpdatePreviewCamera();
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        isOrbitDragging_ = false;
    }
    
    // 右ドラッグでパン
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        if (!isPanDragging_) {
            isPanDragging_ = true;
            lastMousePos_ = io.MousePos;
        }
        
        float deltaX = io.MousePos.x - lastMousePos_.x;
        float deltaY = io.MousePos.y - lastMousePos_.y;
        
        // カメラのright/up方向に沿ってパン
        float cosAngle = std::cos(previewOrbitAngle_);
        float sinAngle = std::sin(previewOrbitAngle_);
        
        float panSpeed = previewOrbitDistance_ * 0.002f;
        previewOrbitTarget_.x += (cosAngle * deltaX) * panSpeed;
        previewOrbitTarget_.z += (sinAngle * deltaX) * panSpeed;
        previewOrbitTarget_.y += deltaY * panSpeed;  // 上ドラッグで下に移動
        
        lastMousePos_ = io.MousePos;
        UpdatePreviewCamera();
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        isPanDragging_ = false;
    }
}

void ParticleEditor::DrawGrid(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    ImU32 gridColor = IM_COL32(60, 60, 70, 100);
    ImU32 axisColorX = IM_COL32(200, 60, 60, 200);  // X軸（赤）
    ImU32 axisColorZ = IM_COL32(60, 60, 200, 200);  // Z軸（青）
    ImU32 axisColorY = IM_COL32(60, 200, 60, 200);  // Y軸（緑）

    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;
    float baseScale = canvasSize.y * 0.08f;
    float scale = baseScale * (12.0f / previewOrbitDistance_);  // ズーム連動

    // 3D変換用
    Transform3D transform;
    transform.centerX = centerX;
    transform.centerY = centerY;
    transform.scale = scale;
    transform.camAngle = previewOrbitAngle_;
    transform.camPitch = previewOrbitPitch_;
    transform.orbitDistance = previewOrbitDistance_;

    // グリッドサイズ
    float gridSize = 10.0f;
    int gridCount = 10;
    float gridStep = gridSize / gridCount;

    // XZ平面にグリッド描画（3D）
    for (int i = -gridCount; i <= gridCount; ++i) {
        float offset = i * gridStep;
        ImU32 color = (i == 0) ? axisColorX : gridColor;

        // X方向の線（Z軸に沿って）
        ImVec2 p1 = transform.project(offset, 0.0f, -gridSize);
        ImVec2 p2 = transform.project(offset, 0.0f, gridSize);
        drawList->AddLine(p1, p2, color, (i == 0) ? 2.0f : 1.0f);

        // Z方向の線（X軸に沿って）
        color = (i == 0) ? axisColorZ : gridColor;
        p1 = transform.project(-gridSize, 0.0f, offset);
        p2 = transform.project(gridSize, 0.0f, offset);
        drawList->AddLine(p1, p2, color, (i == 0) ? 2.0f : 1.0f);
    }

    // Y軸を描画（上方向）
    if (showAxis_) {
        ImVec2 origin = transform.project(0.0f, 0.0f, 0.0f);
        ImVec2 yAxisEnd = transform.project(0.0f, 5.0f, 0.0f);
        drawList->AddLine(origin, yAxisEnd, axisColorY, 2.0f);

        // 軸ラベル
        ImVec2 xLabel = transform.project(gridSize + 0.5f, 0.0f, 0.0f);
        ImVec2 yLabel = transform.project(0.0f, 5.5f, 0.0f);
        ImVec2 zLabel = transform.project(0.0f, 0.0f, gridSize + 0.5f);
        drawList->AddText(xLabel, axisColorX, "X");
        drawList->AddText(yLabel, axisColorY, "Y");
        drawList->AddText(zLabel, axisColorZ, "Z");
    }
}

void ParticleEditor::DrawEmitterProperties() {
    ImGui::BeginChild("Properties", ImVec2(0, 0), true);

    if (!selectedEmitter_) {
        ImGui::Text("エミッターを選択してください");
        ImGui::EndChild();
        return;
    }

    auto& config = selectedEmitter_->GetConfig();

    // 名前
    char nameBuffer[256];
    strncpy_s(nameBuffer, config.name.c_str(), sizeof(nameBuffer) - 1);
    if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer))) {
        config.name = nameBuffer;
        hasUnsavedChanges_ = true;
    }

    ImGui::Separator();

    // 基本設定
    if (ImGui::CollapsingHeader("基本設定", ImGuiTreeNodeFlags_DefaultOpen)) {
        hasUnsavedChanges_ |= ImGui::DragFloat("再生時間", &config.duration, 0.1f, 0.1f, 100.0f);
        hasUnsavedChanges_ |= ImGui::Checkbox("ループ", &config.looping);
        hasUnsavedChanges_ |= ImGui::Checkbox("プリウォーム", &config.prewarm);
        hasUnsavedChanges_ |= ImGui::DragFloat("開始遅延", &config.startDelay, 0.01f, 0.0f, 10.0f);
        hasUnsavedChanges_ |= ImGui::DragInt("最大パーティクル数", reinterpret_cast<int*>(&config.maxParticles), 10, 1, 100000);
    }

    // タブベースのプロパティ
    if (ImGui::BeginTabBar("PropertyTabs")) {
        if (ImGui::BeginTabItem("放出")) {
            DrawEmissionSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("形状")) {
            DrawShapeSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("速度")) {
            DrawVelocitySection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("カラー")) {
            DrawColorSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("サイズ")) {
            DrawSizeSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("回転")) {
            DrawRotationSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("衝突")) {
            DrawCollisionSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("描画")) {
            DrawRenderingSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("サブエミッター")) {
            DrawSubEmitterSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("力場")) {
            DrawForceFieldSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("アトラクター")) {
            DrawAttractorSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("軌道")) {
            DrawOrbitalSection(config);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("リボン")) {
            DrawRibbonSection(config);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

void ParticleEditor::DrawEmissionSection(EmitterConfig& config) {
    ImGui::Text("時間あたりの放出数");
    hasUnsavedChanges_ |= ImGui::DragFloat("放出レート", &config.emitRate, 1.0f, 0.0f, 10000.0f);

    ImGui::Separator();
    ImGui::Text("バースト");
    hasUnsavedChanges_ |= ParticleWidgets::BurstEditor("##Bursts", config.bursts);
}

void ParticleEditor::DrawShapeSection(EmitterConfig& config) {
    auto& shape = config.shape;

    hasUnsavedChanges_ |= ParticleWidgets::ShapeCombo("形状", shape.shape);

    switch (shape.shape) {
    case EmitShape::Sphere:
    case EmitShape::Hemisphere:
    case EmitShape::Circle:
        hasUnsavedChanges_ |= ImGui::DragFloat("半径", &shape.radius, 0.1f, 0.0f, 100.0f);
        break;

    case EmitShape::Box:
        hasUnsavedChanges_ |= ParticleWidgets::Vector3Input("ボックスサイズ", shape.boxSize);
        break;

    case EmitShape::Cone:
        hasUnsavedChanges_ |= ParticleWidgets::AngleSlider("角度", shape.coneAngle, 0.0f, 90.0f);
        hasUnsavedChanges_ |= ImGui::DragFloat("半径", &shape.coneRadius, 0.1f, 0.0f, 100.0f);
        break;

    default:
        break;
    }

    if (shape.shape == EmitShape::Circle || shape.shape == EmitShape::Cone) {
        hasUnsavedChanges_ |= ParticleWidgets::AngleSlider("円弧", shape.arcAngle, 0.0f, 360.0f);
    }

    hasUnsavedChanges_ |= ImGui::Checkbox("エッジから放出", &shape.emitFromEdge);
    hasUnsavedChanges_ |= ImGui::Checkbox("ランダム方向", &shape.randomDirection);

    ImGui::Separator();
    ImGui::Text("位置オフセット");
    hasUnsavedChanges_ |= ParticleWidgets::Vector3Input("位置", shape.position);
    hasUnsavedChanges_ |= ParticleWidgets::Vector3Input("回転", shape.rotation);
}

void ParticleEditor::DrawVelocitySection(EmitterConfig& config) {
    ImGui::Text("初速");
    hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("初速", config.startSpeed, 0.0f, 50.0f);

    ImGui::Separator();

    hasUnsavedChanges_ |= ImGui::Checkbox("ライフタイム中の速度", &config.velocityOverLifetime.enabled);
    if (config.velocityOverLifetime.enabled) {
        ImGui::Indent();
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("速度倍率",
            config.velocityOverLifetime.speedMultiplier, 0.0f, 2.0f);
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("X", config.velocityOverLifetime.x, -10.0f, 10.0f);
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("Y", config.velocityOverLifetime.y, -10.0f, 10.0f);
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("Z", config.velocityOverLifetime.z, -10.0f, 10.0f);
        hasUnsavedChanges_ |= ImGui::Checkbox("ローカル空間", &config.velocityOverLifetime.isLocal);
        ImGui::Unindent();
    }

    ImGui::Separator();

    hasUnsavedChanges_ |= ImGui::Checkbox("ライフタイム中の力", &config.forceOverLifetime.enabled);
    if (config.forceOverLifetime.enabled) {
        ImGui::Indent();
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("X", config.forceOverLifetime.x, -50.0f, 50.0f);
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("Y", config.forceOverLifetime.y, -50.0f, 50.0f);
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("Z", config.forceOverLifetime.z, -50.0f, 50.0f);
        hasUnsavedChanges_ |= ImGui::Checkbox("ローカル空間", &config.forceOverLifetime.isLocal);
        ImGui::Unindent();
    }
}

void ParticleEditor::DrawColorSection(EmitterConfig& config) {
    ImGui::Text("初期カラー");
    hasUnsavedChanges_ |= CurveEditor::DrawMinMaxGradient("初期カラー", config.startColor);

    ImGui::Separator();

    hasUnsavedChanges_ |= ImGui::Checkbox("ライフタイム中のカラー", &config.colorOverLifetime.enabled);
    if (config.colorOverLifetime.enabled) {
        ImGui::Indent();
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxGradient("カラー", config.colorOverLifetime.color);
        ImGui::Unindent();
    }
}

void ParticleEditor::DrawSizeSection(EmitterConfig& config) {
    ImGui::Text("初期サイズ");
    hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("初期サイズ", config.startSize, 0.0f, 10.0f);

    ImGui::Separator();

    hasUnsavedChanges_ |= ImGui::Checkbox("ライフタイム中のサイズ", &config.sizeOverLifetime.enabled);
    if (config.sizeOverLifetime.enabled) {
        ImGui::Indent();
        hasUnsavedChanges_ |= ImGui::Checkbox("軸別に設定", &config.sizeOverLifetime.separateAxes);

        if (config.sizeOverLifetime.separateAxes) {
            hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("X", config.sizeOverLifetime.x, 0.0f, 2.0f);
            hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("Y", config.sizeOverLifetime.y, 0.0f, 2.0f);
        } else {
            hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("サイズ", config.sizeOverLifetime.size, 0.0f, 2.0f);
        }
        ImGui::Unindent();
    }
}

void ParticleEditor::DrawRotationSection(EmitterConfig& config) {
    ImGui::Text("初期回転");
    hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("初期回転", config.startRotation, 0.0f, 360.0f);

    ImGui::Separator();

    hasUnsavedChanges_ |= ImGui::Checkbox("ライフタイム中の回転", &config.rotationOverLifetime.enabled);
    if (config.rotationOverLifetime.enabled) {
        ImGui::Indent();
        hasUnsavedChanges_ |= CurveEditor::DrawMinMaxCurve("角速度 (度/秒)",
            config.rotationOverLifetime.angularVelocity, -360.0f, 360.0f);
        ImGui::Unindent();
    }
}

void ParticleEditor::DrawCollisionSection(EmitterConfig& config) {
    auto& collision = config.collision;

    hasUnsavedChanges_ |= ImGui::Checkbox("衝突を有効化", &collision.enabled);

    if (collision.enabled) {
        hasUnsavedChanges_ |= ImGui::SliderFloat("反発係数", &collision.bounce, 0.0f, 1.0f);
        hasUnsavedChanges_ |= ImGui::SliderFloat("寿命減少率", &collision.lifetimeLoss, 0.0f, 1.0f);
        hasUnsavedChanges_ |= ImGui::DragFloat("消滅速度閾値", &collision.minKillSpeed, 0.1f, 0.0f, 10.0f);
        hasUnsavedChanges_ |= ImGui::Checkbox("衝突時に消滅", &collision.killOnCollision);
        hasUnsavedChanges_ |= ImGui::SliderFloat("半径スケール", &collision.radiusScale, 0.1f, 2.0f);
    }
}

void ParticleEditor::DrawRenderingSection(EmitterConfig& config) {
    hasUnsavedChanges_ |= ParticleWidgets::RenderModeCombo("描画モード", config.renderMode);
    hasUnsavedChanges_ |= ParticleWidgets::BlendModeCombo("ブレンドモード", config.blendMode);

    ImGui::Separator();
    ImGui::Text("プロシージャル形状（テクスチャ不要）");

    const char* shapeNames[] = {
        "なし", "円", "リング", "星", "五角形",
        "六角形", "魔法陣", "ルーン", "十字", "きらめき"
    };
    int shapeIndex = static_cast<int>(config.proceduralShape);
    if (ImGui::Combo("形状タイプ", &shapeIndex, shapeNames, IM_ARRAYSIZE(shapeNames))) {
        config.proceduralShape = static_cast<ProceduralShape>(shapeIndex);
        hasUnsavedChanges_ = true;
    }

    if (config.proceduralShape != ProceduralShape::None) {
        ImGui::Indent();

        switch (config.proceduralShape) {
        case ProceduralShape::Ring:
            hasUnsavedChanges_ |= ImGui::SliderFloat("リング太さ", &config.proceduralParam1, 0.05f, 0.5f);
            break;
        case ProceduralShape::Star:
            hasUnsavedChanges_ |= ImGui::SliderFloat("内側比率", &config.proceduralParam1, 0.1f, 0.9f);
            hasUnsavedChanges_ |= ImGui::SliderFloat("頂点数", &config.proceduralParam2, 3.0f, 12.0f);
            break;
        case ProceduralShape::MagicCircle:
            hasUnsavedChanges_ |= ImGui::SliderFloat("複雑さ", &config.proceduralParam1, 0.3f, 1.0f);
            hasUnsavedChanges_ |= ImGui::SliderFloat("装飾数", &config.proceduralParam2, 4.0f, 12.0f);
            break;
        default:
            break;
        }

        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::Text("テクスチャ");

    char texPath[512];
    strncpy_s(texPath, config.texturePath.c_str(), sizeof(texPath) - 1);
    if (ImGui::InputText("テクスチャパス", texPath, sizeof(texPath))) {
        config.texturePath = texPath;
        hasUnsavedChanges_ = true;
    }

    ImGui::Separator();

    auto& spriteSheet = config.spriteSheet;
    hasUnsavedChanges_ |= ImGui::Checkbox("スプライトシートアニメーション", &spriteSheet.enabled);

    if (spriteSheet.enabled) {
        ImGui::Indent();
        hasUnsavedChanges_ |= ImGui::DragInt("横タイル数", &spriteSheet.tilesX, 1, 1, 16);
        hasUnsavedChanges_ |= ImGui::DragInt("縦タイル数", &spriteSheet.tilesY, 1, 1, 16);
        hasUnsavedChanges_ |= ImGui::DragInt("フレーム数", &spriteSheet.frameCount, 1, 1, 256);
        hasUnsavedChanges_ |= ImGui::DragFloat("FPS", &spriteSheet.fps, 1.0f, 1.0f, 60.0f);
        hasUnsavedChanges_ |= ImGui::DragInt("開始フレーム", &spriteSheet.startFrame, 1, 0, spriteSheet.frameCount - 1);
        hasUnsavedChanges_ |= ImGui::Checkbox("ループ", &spriteSheet.loop);
        ImGui::Unindent();
    }
}

void ParticleEditor::DrawSubEmitterSection(EmitterConfig& config) {
    ImGui::Text("サブエミッター");

    for (size_t i = 0; i < config.subEmitters.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        auto& sub = config.subEmitters[i];

        ImGui::Separator();

        const char* triggerNames[] = { "生成時", "消滅時", "衝突時" };
        int trigger = static_cast<int>(sub.trigger);
        if (ImGui::Combo("トリガー", &trigger, triggerNames, IM_ARRAYSIZE(triggerNames))) {
            sub.trigger = static_cast<SubEmitterConfig::Trigger>(trigger);
            hasUnsavedChanges_ = true;
        }

        char emitterName[256];
        strncpy_s(emitterName, sub.emitterName.c_str(), sizeof(emitterName) - 1);
        if (ImGui::InputText("エミッター名", emitterName, sizeof(emitterName))) {
            sub.emitterName = emitterName;
            hasUnsavedChanges_ = true;
        }

        hasUnsavedChanges_ |= ImGui::DragInt("放出数", &sub.emitCount, 1, 1, 100);
        hasUnsavedChanges_ |= ImGui::SliderFloat("確率", &sub.probability, 0.0f, 1.0f);

        if (ImGui::Button("削除")) {
            config.subEmitters.erase(config.subEmitters.begin() + i);
            hasUnsavedChanges_ = true;
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    if (ImGui::Button("+ サブエミッター追加")) {
        config.subEmitters.push_back(SubEmitterConfig());
        hasUnsavedChanges_ = true;
    }
}

void ParticleEditor::NewEffect() {
    if (particleSystem_) {
        particleSystem_->RemoveAllEmitters();
        auto* emitter = particleSystem_->CreateEmitter("Main Emitter");
        emitter->Play();
        selectedEmitter_ = emitter;
        selectedEmitterIndex_ = 0;
    }
    currentFilePath_.clear();
    hasUnsavedChanges_ = false;
}

void ParticleEditor::OpenEffect() {
    // TODO: ファイルダイアログでJSONファイルを選択してロード
}

void ParticleEditor::SaveEffect() {
    if (currentFilePath_.empty()) {
        SaveEffectAs();
    } else {
        // TODO: 現在のパスに保存
        hasUnsavedChanges_ = false;
    }
}

void ParticleEditor::SaveEffectAs() {
    // TODO: ファイルダイアログでパスを選択して保存
}

void ParticleEditor::UpdatePreview(float deltaTime) {
    if (!isVisible_ || !isPlaying_) return;

    previewTime_ += deltaTime * playbackSpeed_;
    
    // 自動回転（オプション）
    if (autoRotatePreview_) {
        previewOrbitAngle_ += deltaTime * 0.5f;
    }
    
    // CPUパーティクル更新
    UpdatePreviewParticles(deltaTime * playbackSpeed_);
}

void ParticleEditor::UpdatePreviewCamera() {
    if (!previewCamera_) return;

    // オービットカメラ：球面座標からカメラ位置を計算
    float cosPitch = std::cos(previewOrbitPitch_);
    float sinPitch = std::sin(previewOrbitPitch_);
    float cosAngle = std::cos(previewOrbitAngle_);
    float sinAngle = std::sin(previewOrbitAngle_);

    float x = previewOrbitTarget_.x + previewOrbitDistance_ * cosPitch * sinAngle;
    float y = previewOrbitTarget_.y + previewOrbitDistance_ * sinPitch;
    float z = previewOrbitTarget_.z + previewOrbitDistance_ * cosPitch * cosAngle;

    previewCamera_->SetPosition(Vector3(x, y, z));
    previewCamera_->SetTarget(Vector3(previewOrbitTarget_.x, previewOrbitTarget_.y, previewOrbitTarget_.z));
    
    // アスペクト比を更新
    if (previewWidth_ > 0 && previewHeight_ > 0) {
        float aspectRatio = static_cast<float>(previewWidth_) / static_cast<float>(previewHeight_);
        previewCamera_->SetPerspective(45.0f * 3.14159f / 180.0f, aspectRatio, 0.1f, 1000.0f);
    }
}

void ParticleEditor::UpdatePreviewParticles(float deltaTime) {
    if (!selectedEmitter_) return;
    
    auto& config = selectedEmitter_->GetConfig();
    
    // パーティクル発生
    if (config.emitRate > 0.0f) {
        previewEmitAccumulator_ += config.emitRate * deltaTime;
        int emitCount = static_cast<int>(previewEmitAccumulator_);
        previewEmitAccumulator_ -= emitCount;
        
        if (emitCount > 0) {
            EmitPreviewParticles(emitCount);
        }
    }
    
    // 既存パーティクル更新
    Float3 gravity = { 0.0f, 0.0f, 0.0f };
    if (config.forceOverLifetime.enabled) {
        gravity.x = config.forceOverLifetime.x.Evaluate(0.5f, 0.5f);
        gravity.y = config.forceOverLifetime.y.Evaluate(0.5f, 0.5f);
        gravity.z = config.forceOverLifetime.z.Evaluate(0.5f, 0.5f);
    }
    
    for (auto& p : previewParticles_) {
        if (!p.alive) continue;
        
        p.lifetime += deltaTime;
        if (p.lifetime >= p.maxLifetime) {
            p.alive = false;
            continue;
        }
        
        // ライフタイム進行率
        float t = p.lifetime / p.maxLifetime;
        
        // 基本的な重力/力
        p.velocity.x += gravity.x * deltaTime;
        p.velocity.y += gravity.y * deltaTime;
        p.velocity.z += gravity.z * deltaTime;
        
        // フォースフィールド処理
        if (config.forceField.enabled) {
            for (const auto& field : config.forceField.fields) {
                if (!field.enabled) continue;
                
                Float3 forceDir = { 0.0f, 0.0f, 0.0f };
                float forceMag = field.strength;
                
                // 距離計算
                float dx = p.position.x - field.position.x;
                float dy = p.position.y - field.position.y;
                float dz = p.position.z - field.position.z;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                
                // 減衰
                if (field.attenuation > 0.0f && dist > 0.0f) {
                    forceMag *= std::exp(-field.attenuation * dist);
                }
                
                switch (field.type) {
                case ForceFieldType::Directional:
                    forceDir = field.direction;
                    break;
                    
                case ForceFieldType::Radial:
                    if (dist > 0.01f) {
                        forceDir.x = dx / dist;
                        forceDir.y = dy / dist;
                        forceDir.z = dz / dist;
                    }
                    break;
                    
                case ForceFieldType::Vortex: {
                    // 渦巻き - 接線方向の力 + 内向き力 + 上向き力
                    // 軸からの距離（2D、XZ平面）
                    float rx = p.position.x - field.position.x;
                    float rz = p.position.z - field.position.z;
                    float radialDist = std::sqrt(rx * rx + rz * rz);
                    
                    if (radialDist > 0.01f) {
                        // 接線方向（回転）
                        float tangentX = -rz / radialDist;
                        float tangentZ = rx / radialDist;
                        
                        // 放射方向（内向き/外向き）
                        float radialX = rx / radialDist;
                        float radialZ = rz / radialDist;
                        
                        forceDir.x = tangentX * field.rotationSpeed + radialX * field.inwardForce;
                        forceDir.y = field.upwardForce;
                        forceDir.z = tangentZ * field.rotationSpeed + radialZ * field.inwardForce;
                    }
                    break;
                }
                    
                case ForceFieldType::Turbulence: {
                    // 簡易ノイズベースの乱流
                    float noiseX = std::sin(p.position.x * field.frequency + previewTime_);
                    float noiseY = std::cos(p.position.y * field.frequency + previewTime_ * 0.7f);
                    float noiseZ = std::sin(p.position.z * field.frequency + previewTime_ * 1.3f);
                    forceDir.x = noiseX;
                    forceDir.y = noiseY;
                    forceDir.z = noiseZ;
                    break;
                }
                    
                case ForceFieldType::Drag:
                    // 速度に反比例する抵抗
                    forceDir.x = -p.velocity.x * field.dragCoefficient;
                    forceDir.y = -p.velocity.y * field.dragCoefficient;
                    forceDir.z = -p.velocity.z * field.dragCoefficient;
                    forceMag = 1.0f;  // ドラッグは力の強さを直接適用
                    break;
                }
                
                p.velocity.x += forceDir.x * forceMag * deltaTime;
                p.velocity.y += forceDir.y * forceMag * deltaTime;
                p.velocity.z += forceDir.z * forceMag * deltaTime;
            }
        }
        
        // アトラクター処理
        if (config.attractor.enabled) {
            for (const auto& attr : config.attractor.attractors) {
                if (!attr.enabled) continue;
                
                float dx = attr.position.x - p.position.x;
                float dy = attr.position.y - p.position.y;
                float dz = attr.position.z - p.position.z;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                
                // デッドゾーン内では力が働かない
                if (dist < attr.deadzone) {
                    if (attr.killOnContact) {
                        p.alive = false;
                    }
                    continue;
                }
                
                // 影響範囲外は無視
                if (dist > attr.radius) continue;
                
                // 力の計算（逆二乗則）
                float forceMag = attr.strength / (dist * dist + 0.1f);
                
                // 内側範囲では最大力
                if (dist < attr.innerRadius && attr.innerRadius > 0.0f) {
                    forceMag = attr.strength;
                }
                
                if (dist > 0.01f) {
                    p.velocity.x += (dx / dist) * forceMag * deltaTime;
                    p.velocity.y += (dy / dist) * forceMag * deltaTime;
                    p.velocity.z += (dz / dist) * forceMag * deltaTime;
                }
            }
        }
        
        // 軌道運動
        if (config.orbital.enabled) {
            float angularVel = config.orbital.angularVelocity.Evaluate(t, p.random);
            float radialVel = config.orbital.radialVelocity.Evaluate(t, p.random);
            
            // 中心からの相対位置
            float rx = p.position.x - config.orbital.center.x;
            float rz = p.position.z - config.orbital.center.z;
            float radialDist = std::sqrt(rx * rx + rz * rz);
            
            if (radialDist > 0.01f) {
                // 角度をラジアンに変換して適用
                float angularRad = angularVel * 3.14159f / 180.0f * deltaTime;
                
                // 回転行列適用
                float cosA = std::cos(angularRad);
                float sinA = std::sin(angularRad);
                float newRx = rx * cosA - rz * sinA;
                float newRz = rx * sinA + rz * cosA;
                
                // 半径方向の移動
                if (std::abs(radialVel) > 0.001f) {
                    float newDist = radialDist + radialVel * deltaTime;
                    if (newDist > 0.01f) {
                        newRx = newRx / radialDist * newDist;
                        newRz = newRz / radialDist * newDist;
                    }
                }
                
                p.position.x = config.orbital.center.x + newRx;
                p.position.z = config.orbital.center.z + newRz;
            }
        }
        
        // 位置更新
        p.position.x += p.velocity.x * deltaTime;
        p.position.y += p.velocity.y * deltaTime;
        p.position.z += p.velocity.z * deltaTime;
        
        // サイズ変化（ライフタイム中のサイズ）
        if (config.sizeOverLifetime.enabled) {
            float sizeMultiplier = config.sizeOverLifetime.size.Evaluate(t, p.random);
            p.size = config.startSize.Evaluate(0.0f, p.random) * sizeMultiplier;
        }
        
        // カラー変化（ライフタイム中のカラー）
        if (config.colorOverLifetime.enabled) {
            p.color = config.colorOverLifetime.color.Evaluate(t, p.random);
        } else {
            // フェードアウト
            p.color.w = 1.0f - t;
        }
    }
}

void ParticleEditor::EmitPreviewParticles(int count) {
    if (!selectedEmitter_) return;
    
    auto& config = selectedEmitter_->GetConfig();
    
    for (int i = 0; i < count; ++i) {
        // 空きスロット検索
        PreviewParticle* slot = nullptr;
        for (auto& p : previewParticles_) {
            if (!p.alive) {
                slot = &p;
                break;
            }
        }
        
        // 空きがなければ新規追加
        if (!slot) {
            if (previewParticles_.size() >= MAX_PREVIEW_PARTICLES) {
                // 最も古いパーティクルを再利用
                float maxAge = 0.0f;
                for (auto& p : previewParticles_) {
                    if (p.lifetime > maxAge) {
                        maxAge = p.lifetime;
                        slot = &p;
                    }
                }
            } else {
                previewParticles_.push_back({});
                slot = &previewParticles_.back();
            }
        }
        
        if (!slot) continue;
        
        // パーティクル初期化
        slot->alive = true;
        slot->lifetime = 0.0f;
        slot->random = previewDist_(previewRng_);
        
        // 形状からの位置サンプリング
        Float3 pos = { 0.0f, 0.0f, 0.0f };
        Float3 dir = { 0.0f, 1.0f, 0.0f };
        
        switch (config.shape.shape) {
        case EmitShape::Point:
            pos = { 0.0f, 0.0f, 0.0f };
            break;
        case EmitShape::Sphere:
        case EmitShape::Hemisphere: {
            float theta = previewDist_(previewRng_) * 6.28318f;
            float phi = std::acos(2.0f * previewDist_(previewRng_) - 1.0f);
            if (config.shape.shape == EmitShape::Hemisphere) {
                phi = std::acos(previewDist_(previewRng_));
            }
            float r = config.shape.radius * std::cbrt(previewDist_(previewRng_));
            pos.x = r * std::sin(phi) * std::cos(theta);
            pos.y = r * std::cos(phi);
            pos.z = r * std::sin(phi) * std::sin(theta);
            dir = { pos.x, pos.y, pos.z };
            float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
            if (len > 0.001f) {
                dir.x /= len; dir.y /= len; dir.z /= len;
            }
            break;
        }
        case EmitShape::Cone: {
            float angle = previewDist_(previewRng_) * config.shape.coneAngle * 3.14159f / 180.0f;
            float theta = previewDist_(previewRng_) * 6.28318f;
            dir.x = std::sin(angle) * std::cos(theta);
            dir.y = std::cos(angle);
            dir.z = std::sin(angle) * std::sin(theta);
            pos.x = previewDist_(previewRng_) * config.shape.coneRadius * std::cos(theta);
            pos.z = previewDist_(previewRng_) * config.shape.coneRadius * std::sin(theta);
            break;
        }
        case EmitShape::Box:
            pos.x = (previewDist_(previewRng_) - 0.5f) * config.shape.boxSize.x;
            pos.y = (previewDist_(previewRng_) - 0.5f) * config.shape.boxSize.y;
            pos.z = (previewDist_(previewRng_) - 0.5f) * config.shape.boxSize.z;
            break;
        default:
            break;
        }
        
        slot->position = pos;
        
        // 速度
        float speed = config.startSpeed.Evaluate(0.0f, slot->random);
        slot->velocity = { dir.x * speed, dir.y * speed, dir.z * speed };
        
        // その他の初期値
        slot->maxLifetime = config.startLifetime.Evaluate(0.0f, slot->random);
        slot->size = config.startSize.Evaluate(0.0f, slot->random);
        slot->color = config.startColor.Evaluate(0.0f, slot->random);
        slot->rotation = config.startRotation.Evaluate(0.0f, slot->random);
    }
}

// プロシージャル形状をImDrawListで描画するヘルパー関数（3D対応版）
namespace {
    // 3D魔法陣を水平面(XZ平面)に描画
    void DrawMagicCircle3D(ImDrawList* drawList, const Transform3D& transform,
                           float posY, float radius, float time, ImU32 baseColor, float alpha) {
        int r = (baseColor >> 0) & 0xFF;
        int g = (baseColor >> 8) & 0xFF;
        int b = (baseColor >> 16) & 0xFF;

        float angle1 = time * 0.5f;
        float angle2 = -time * 0.8f;
        float angle3 = time * 0.3f;

        // 外側リング（XZ平面上の円）
        ImU32 ringColor = IM_COL32(r, g, b, static_cast<int>(alpha * 220));
        std::vector<ImVec2> outerRingPts;
        int segments = 64;
        for (int i = 0; i < segments; i++) {
            float a = i * 6.28318f / segments;
            float x = radius * 0.95f * std::cos(a);
            float z = radius * 0.95f * std::sin(a);
            outerRingPts.push_back(transform.project(x, posY, z));
        }
        for (int i = 0; i < segments; i++) {
            drawList->AddLine(outerRingPts[i], outerRingPts[(i + 1) % segments], ringColor, 3.0f);
        }

        // 外側リング2
        std::vector<ImVec2> outerRing2Pts;
        for (int i = 0; i < segments; i++) {
            float a = i * 6.28318f / segments;
            float x = radius * 0.90f * std::cos(a);
            float z = radius * 0.90f * std::sin(a);
            outerRing2Pts.push_back(transform.project(x, posY, z));
        }
        ImU32 ring2Color = IM_COL32(r, g, b, static_cast<int>(alpha * 150));
        for (int i = 0; i < segments; i++) {
            drawList->AddLine(outerRing2Pts[i], outerRing2Pts[(i + 1) % segments], ring2Color, 1.5f);
        }

        // 装飾ドット（外周を回転）
        int numDots = 12;
        for (int i = 0; i < numDots; i++) {
            float a = angle1 + i * 6.28318f / numDots;
            float x = radius * 0.92f * std::cos(a);
            float z = radius * 0.92f * std::sin(a);
            ImVec2 screenPos = transform.project(x, posY, z);
            drawList->AddCircleFilled(screenPos, 4.0f, ringColor, 8);
        }

        // 中間リング
        std::vector<ImVec2> midRingPts;
        for (int i = 0; i < segments; i++) {
            float a = i * 6.28318f / segments;
            float x = radius * 0.70f * std::cos(a);
            float z = radius * 0.70f * std::sin(a);
            midRingPts.push_back(transform.project(x, posY, z));
        }
        ImU32 midColor = IM_COL32(r * 0.8f, g * 0.7f, b, static_cast<int>(alpha * 180));
        for (int i = 0; i < segments; i++) {
            drawList->AddLine(midRingPts[i], midRingPts[(i + 1) % segments], midColor, 2.0f);
        }

        // 六芒星（回転）
        ImU32 starColor = IM_COL32(r, g * 0.8f, b, static_cast<int>(alpha * 180));
        for (int star = 0; star < 2; star++) {
            std::vector<ImVec2> starPts;
            for (int i = 0; i < 3; i++) {
                float a = angle2 + star * 3.14159f / 6 + i * 6.28318f / 3;
                float x = radius * 0.60f * std::cos(a);
                float z = radius * 0.60f * std::sin(a);
                starPts.push_back(transform.project(x, posY, z));
            }
            drawList->AddTriangle(starPts[0], starPts[1], starPts[2], starColor, 2.5f);
        }

        // 五芒星（逆回転）
        ImU32 innerStarColor = IM_COL32(r * 0.9f, g, b * 0.9f, static_cast<int>(alpha * 150));
        std::vector<ImVec2> pentaPts;
        for (int i = 0; i < 5; i++) {
            float a = angle3 + i * 6.28318f / 5 - 3.14159f / 2;
            float x = radius * 0.45f * std::cos(a);
            float z = radius * 0.45f * std::sin(a);
            pentaPts.push_back(transform.project(x, posY, z));
        }
        // 五芒星の線を描画
        for (int i = 0; i < 5; i++) {
            drawList->AddLine(pentaPts[i], pentaPts[(i + 2) % 5], innerStarColor, 2.0f);
        }

        // 放射状ライン
        ImU32 lineColor = IM_COL32(r, g, b, static_cast<int>(alpha * 100));
        for (int i = 0; i < 8; i++) {
            float a = angle1 + i * 6.28318f / 8;
            float x1 = radius * 0.15f * std::cos(a);
            float z1 = radius * 0.15f * std::sin(a);
            float x2 = radius * 0.65f * std::cos(a);
            float z2 = radius * 0.65f * std::sin(a);
            ImVec2 p1 = transform.project(x1, posY, z1);
            ImVec2 p2 = transform.project(x2, posY, z2);
            drawList->AddLine(p1, p2, lineColor, 1.5f);
        }

        // 内側リング
        std::vector<ImVec2> innerRingPts;
        for (int i = 0; i < 32; i++) {
            float a = i * 6.28318f / 32;
            float x = radius * 0.25f * std::cos(a);
            float z = radius * 0.25f * std::sin(a);
            innerRingPts.push_back(transform.project(x, posY, z));
        }
        ImU32 innerRingColor = IM_COL32(255, 255, 255, static_cast<int>(alpha * 200));
        for (int i = 0; i < 32; i++) {
            drawList->AddLine(innerRingPts[i], innerRingPts[(i + 1) % 32], innerRingColor, 2.0f);
        }

        // 中心グロー
        ImVec2 centerScreen = transform.project(0, posY, 0);
        ImU32 glowColor1 = IM_COL32(r, g, b, static_cast<int>(alpha * 60));
        ImU32 glowColor2 = IM_COL32(255, 255, 255, static_cast<int>(alpha * 150));
        drawList->AddCircleFilled(centerScreen, 20.0f, glowColor1, 16);
        drawList->AddCircleFilled(centerScreen, 10.0f, glowColor2, 16);

        // 垂直のエネルギービーム（Y軸方向）
        ImU32 beamColor = IM_COL32(r, g, b, static_cast<int>(alpha * 80));
        ImVec2 beamTop = transform.project(0, posY + radius * 1.5f, 0);
        ImVec2 beamBottom = transform.project(0, posY - radius * 0.2f, 0);
        drawList->AddLine(centerScreen, beamTop, beamColor, 8.0f);

        // ビームのグロー
        ImU32 beamGlow = IM_COL32(r, g, b, static_cast<int>(alpha * 30));
        drawList->AddLine(centerScreen, beamTop, beamGlow, 20.0f);

        // 浮遊するルーン（3D空間の異なる高さに配置）
        int numRunes = 6;
        for (int i = 0; i < numRunes; i++) {
            float runeAngle = angle2 * 0.5f + i * 6.28318f / numRunes;
            float runeRadius = radius * 0.75f;
            float runeHeight = posY + 1.5f + 0.5f * std::sin(time * 2.0f + i * 1.2f);
            float runeX = runeRadius * std::cos(runeAngle);
            float runeZ = runeRadius * std::sin(runeAngle);

            ImVec2 runePos = transform.project(runeX, runeHeight, runeZ);
            ImU32 runeColor = IM_COL32(r, g * 0.8f, b, static_cast<int>(alpha * 180));

            // ルーン記号風の描画（簡易的な線）
            float runeSize = 8.0f;
            drawList->AddLine(
                ImVec2(runePos.x - runeSize, runePos.y),
                ImVec2(runePos.x + runeSize, runePos.y), runeColor, 2.0f);
            drawList->AddLine(
                ImVec2(runePos.x, runePos.y - runeSize * 1.5f),
                ImVec2(runePos.x, runePos.y + runeSize * 1.5f), runeColor, 2.0f);
            drawList->AddLine(
                ImVec2(runePos.x - runeSize * 0.7f, runePos.y - runeSize * 0.5f),
                ImVec2(runePos.x + runeSize * 0.7f, runePos.y + runeSize * 0.5f), runeColor, 1.5f);

            // グロー
            ImU32 runeGlow = IM_COL32(r, g, b, static_cast<int>(alpha * 40));
            drawList->AddCircleFilled(runePos, 12.0f, runeGlow, 8);
        }

        // 上昇するエネルギーパーティクル
        int numEnergyParticles = 8;
        for (int i = 0; i < numEnergyParticles; i++) {
            float phase = time * 0.8f + i * 0.5f;
            float particleY = posY + std::fmod(phase, 3.0f) * 2.0f;
            float particleAngle = i * 6.28318f / numEnergyParticles + time * 0.3f;
            float particleRadius = radius * 0.3f * (1.0f + 0.3f * std::sin(time + i));
            float px = particleRadius * std::cos(particleAngle);
            float pz = particleRadius * std::sin(particleAngle);

            ImVec2 particlePos = transform.project(px, particleY, pz);
            float fadeAlpha = 1.0f - std::fmod(phase, 3.0f) / 3.0f;
            ImU32 particleColor = IM_COL32(255, 255, 255, static_cast<int>(alpha * 200 * fadeAlpha));
            drawList->AddCircleFilled(particlePos, 3.0f, particleColor, 6);
        }

        // 追加の水平リング（異なる高さに）
        float ring2Y = posY + 2.0f + 0.3f * std::sin(time);
        ImU32 upperRingColor = IM_COL32(r, g, b, static_cast<int>(alpha * 80));
        std::vector<ImVec2> upperRingPts;
        for (int i = 0; i < 32; i++) {
            float a = i * 6.28318f / 32 + angle1 * 2.0f;
            float rx = radius * 0.4f * std::cos(a);
            float rz = radius * 0.4f * std::sin(a);
            upperRingPts.push_back(transform.project(rx, ring2Y, rz));
        }
        for (int i = 0; i < 32; i++) {
            drawList->AddLine(upperRingPts[i], upperRingPts[(i + 1) % 32], upperRingColor, 1.5f);
        }
    }

    // 3Dリングを水平面に描画
    void DrawRing3D(ImDrawList* drawList, const Transform3D& transform,
                    float posY, float outerR, float innerR, ImU32 color, int segments = 48) {
        std::vector<ImVec2> outerPts, innerPts;
        for (int i = 0; i < segments; i++) {
            float a = i * 6.28318f / segments;
            outerPts.push_back(transform.project(outerR * std::cos(a), posY, outerR * std::sin(a)));
            innerPts.push_back(transform.project(innerR * std::cos(a), posY, innerR * std::sin(a)));
        }
        for (int i = 0; i < segments; i++) {
            drawList->AddLine(outerPts[i], outerPts[(i + 1) % segments], color, 2.0f);
            drawList->AddLine(innerPts[i], innerPts[(i + 1) % segments], color, 1.5f);
        }
    }

    // 2D用：星形を描画
    void DrawStar(ImDrawList* drawList, ImVec2 center, float outerR, float innerR, int points, float rotation, ImU32 color) {
        std::vector<ImVec2> pts;
        for (int i = 0; i < points * 2; i++) {
            float angle = rotation + i * 3.14159f / points - 3.14159f / 2;
            float r = (i % 2 == 0) ? outerR : innerR;
            pts.push_back(ImVec2(center.x + r * std::cos(angle), center.y + r * std::sin(angle)));
        }
        drawList->AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), color);
    }

    // 2D用：魔法陣を描画（フォールバック）
    void DrawMagicCircle(ImDrawList* drawList, ImVec2 center, float size, float time, ImU32 baseColor, float alpha) {
        int r = (baseColor >> 0) & 0xFF;
        int g = (baseColor >> 8) & 0xFF;
        int b = (baseColor >> 16) & 0xFF;

        float angle1 = time * 0.5f;
        float angle2 = -time * 0.8f;
        float angle3 = time * 0.3f;

        ImU32 ringColor = IM_COL32(r, g, b, static_cast<int>(alpha * 200));
        drawList->AddCircle(center, size * 0.95f, ringColor, 48, size * 0.05f);

        int numDots = 12;
        for (int i = 0; i < numDots; i++) {
            float a = angle1 + i * 6.28318f / numDots;
            ImVec2 dotPos(center.x + size * 0.9f * std::cos(a), center.y + size * 0.9f * std::sin(a));
            drawList->AddCircleFilled(dotPos, size * 0.04f, ringColor, 8);
        }

        ImU32 midColor = IM_COL32(r * 0.8f, g * 0.7f, b, static_cast<int>(alpha * 180));
        drawList->AddCircle(center, size * 0.72f, midColor, 36, size * 0.03f);

        ImU32 starColor = IM_COL32(r, g * 0.8f, b, static_cast<int>(alpha * 150));
        DrawStar(drawList, center, size * 0.6f, size * 0.3f, 6, angle2, starColor);

        ImU32 innerStarColor = IM_COL32(r * 0.9f, g, b * 0.9f, static_cast<int>(alpha * 120));
        DrawStar(drawList, center, size * 0.45f, size * 0.2f, 5, angle3, innerStarColor);

        ImU32 lineColor = IM_COL32(r, g, b, static_cast<int>(alpha * 80));
        for (int i = 0; i < 8; i++) {
            float a = angle1 + i * 6.28318f / 8;
            ImVec2 p1(center.x + size * 0.2f * std::cos(a), center.y + size * 0.2f * std::sin(a));
            ImVec2 p2(center.x + size * 0.65f * std::cos(a), center.y + size * 0.65f * std::sin(a));
            drawList->AddLine(p1, p2, lineColor, 1.5f);
        }

        ImU32 centerColor = IM_COL32(255, 255, 255, static_cast<int>(alpha * 200));
        drawList->AddCircleFilled(center, size * 0.15f, centerColor, 16);

        ImU32 glowColor = IM_COL32(r, g, b, static_cast<int>(alpha * 100));
        drawList->AddCircleFilled(center, size * 0.25f, glowColor, 16);
    }

    // リングを描画
    void DrawRing(ImDrawList* drawList, ImVec2 center, float outerR, float innerR, ImU32 color, int segments = 32) {
        drawList->AddCircle(center, (outerR + innerR) * 0.5f, color, segments, outerR - innerR);
    }

    // ルーンを描画
    void DrawRune(ImDrawList* drawList, ImVec2 center, float size, float time, ImU32 color) {
        float pulse = 0.8f + 0.2f * std::sin(time * 3.0f);
        int a = static_cast<int>((color >> 24) * pulse);
        ImU32 pulseColor = (color & 0x00FFFFFF) | (a << 24);

        drawList->AddLine(
            ImVec2(center.x, center.y - size * 0.8f),
            ImVec2(center.x, center.y + size * 0.8f),
            pulseColor, size * 0.1f);

        drawList->AddLine(
            ImVec2(center.x - size * 0.4f, center.y - size * 0.3f),
            ImVec2(center.x + size * 0.4f, center.y + size * 0.3f),
            pulseColor, size * 0.08f);

        drawList->AddLine(
            ImVec2(center.x - size * 0.3f, center.y - size * 0.3f),
            ImVec2(center.x + size * 0.3f, center.y - size * 0.3f),
            pulseColor, size * 0.06f);
    }
}

void ParticleEditor::DrawPreviewParticles(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;
    float baseScale = canvasSize.y * 0.08f;
    float scale = baseScale * (12.0f / previewOrbitDistance_);  // ズーム連動

    // カメラ行列の計算（簡易版）
    float camAngle = previewOrbitAngle_;
    float camPitch = previewOrbitPitch_;
    float cosCam = std::cos(camAngle);
    float sinCam = std::sin(camAngle);
    float cosPitch = std::cos(camPitch);
    float sinPitch = std::sin(camPitch);

    // 3D変換用構造体を作成
    Transform3D transform;
    transform.centerX = centerX;
    transform.centerY = centerY;
    transform.scale = scale;
    transform.camAngle = camAngle;
    transform.camPitch = camPitch;
    transform.orbitDistance = previewOrbitDistance_;

    // 魔法陣エミッターがあるかチェック
    bool hasMagicCircleEmitter = false;
    if (particleSystem_) {
        for (size_t i = 0; i < particleSystem_->GetEmitterCount(); i++) {
            auto* emitter = particleSystem_->GetEmitter(i);
            if (emitter && emitter->GetConfig().proceduralShape == ProceduralShape::MagicCircle) {
                hasMagicCircleEmitter = true;
                break;
            }
        }
    }

    // 魔法陣を最初に描画（背景として）
    if (hasMagicCircleEmitter) {
        ImU32 magicColor = IM_COL32(80, 150, 255, 255);
        DrawMagicCircle3D(drawList, transform, 0.0f, 6.0f, previewTime_, magicColor, 1.0f);
    }

    if (previewParticles_.empty()) return;

    // 現在のエミッター設定を取得
    ProceduralShape procShape = ProceduralShape::None;
    float procParam1 = 0.5f;
    float procParam2 = 5.0f;
    if (selectedEmitter_) {
        procShape = selectedEmitter_->GetConfig().proceduralShape;
        procParam1 = selectedEmitter_->GetConfig().proceduralParam1;
        procParam2 = selectedEmitter_->GetConfig().proceduralParam2;
    }
    
    // 深度ソート用の構造体
    struct SortedParticle {
        float depth;
        size_t index;
    };
    std::vector<SortedParticle> sortedIndices;
    sortedIndices.reserve(previewParticles_.size());
    
    // 深度計算とソート準備
    for (size_t i = 0; i < previewParticles_.size(); i++) {
        const auto& p = previewParticles_[i];
        if (!p.alive) continue;
        
        // ビュー空間への変換
        float viewX = p.position.x * cosCam - p.position.z * sinCam;
        float viewZ = p.position.x * sinCam + p.position.z * cosCam;
        float viewY = p.position.y * cosPitch - viewZ * sinPitch;
        float finalZ = p.position.y * sinPitch + viewZ * cosPitch;
        
        sortedIndices.push_back({ finalZ, i });
    }
    
    // 深度でソート（奥から描画）
    std::sort(sortedIndices.begin(), sortedIndices.end(),
        [](const SortedParticle& a, const SortedParticle& b) {
            return a.depth < b.depth;
        });
    
    // パーティクル描画
    for (const auto& sorted : sortedIndices) {
        const auto& p = previewParticles_[sorted.index];
        
        // ビュー空間への変換
        float viewX = p.position.x * cosCam - p.position.z * sinCam;
        float viewZ = p.position.x * sinCam + p.position.z * cosCam;
        float viewY = p.position.y * cosPitch - viewZ * sinPitch;
        float finalZ = p.position.y * sinPitch + viewZ * cosPitch;
        
        // 透視投影
        float depth = finalZ + previewOrbitDistance_;
        if (depth < 0.5f) depth = 0.5f;
        float depthScale = previewOrbitDistance_ / depth;
        
        float screenX = centerX + viewX * scale * depthScale;
        float screenY = centerY - viewY * scale * depthScale;
        
        // 画面外チェック
        if (screenX < canvasPos.x - 50 || screenX > canvasPos.x + canvasSize.x + 50 ||
            screenY < canvasPos.y - 50 || screenY > canvasPos.y + canvasSize.y + 50) {
            continue;
        }
        
        // サイズ（深度によるスケール）
        float size = p.size * scale * 0.5f * depthScale;
        if (size < 1.0f) size = 1.0f;
        if (size > 80.0f) size = 80.0f;
        
        // 深度によるフォグ効果
        float fogFactor = 1.0f;
        if (depth > previewOrbitDistance_) {
            fogFactor = (std::max)(0.3f, 1.0f - (depth - previewOrbitDistance_) / 30.0f);
        }
        
        // カラー
        int r = static_cast<int>(p.color.x * 255 * fogFactor);
        int g = static_cast<int>(p.color.y * 255 * fogFactor);
        int b = static_cast<int>(p.color.z * 255 * fogFactor);
        int a = static_cast<int>(p.color.w * 255);
        ImU32 color = IM_COL32(r, g, b, a);
        ImVec2 screenPos(screenX, screenY);

        // プロシージャル形状に応じた描画
        switch (procShape) {
        case ProceduralShape::MagicCircle:
            DrawMagicCircle(drawList, screenPos, size * 2.0f, previewTime_, color, p.color.w * fogFactor);
            break;

        case ProceduralShape::Ring:
            DrawRing(drawList, screenPos, size, size * (1.0f - procParam1), color, 24);
            break;

        case ProceduralShape::Star: {
            int points = static_cast<int>(procParam2);
            DrawStar(drawList, screenPos, size, size * procParam1, points, p.rotation, color);
            break;
        }

        case ProceduralShape::Pentagon: {
            std::vector<ImVec2> pts;
            for (int i = 0; i < 5; i++) {
                float angle = p.rotation + i * 6.28318f / 5 - 3.14159f / 2;
                pts.push_back(ImVec2(screenX + size * std::cos(angle), screenY + size * std::sin(angle)));
            }
            drawList->AddConvexPolyFilled(pts.data(), 5, color);
            break;
        }

        case ProceduralShape::Hexagon: {
            std::vector<ImVec2> pts;
            for (int i = 0; i < 6; i++) {
                float angle = p.rotation + i * 6.28318f / 6;
                pts.push_back(ImVec2(screenX + size * std::cos(angle), screenY + size * std::sin(angle)));
            }
            drawList->AddConvexPolyFilled(pts.data(), 6, color);
            break;
        }

        case ProceduralShape::Rune:
            DrawRune(drawList, screenPos, size, previewTime_, color);
            break;

        case ProceduralShape::Cross:
            drawList->AddLine(ImVec2(screenX - size, screenY), ImVec2(screenX + size, screenY), color, size * 0.3f);
            drawList->AddLine(ImVec2(screenX, screenY - size), ImVec2(screenX, screenY + size), color, size * 0.3f);
            break;

        case ProceduralShape::Sparkle: {
            // 4方向スパイク
            float pulse = 0.7f + 0.3f * std::sin(previewTime_ * 5.0f);
            ImU32 pulseColor = IM_COL32(r, g, b, static_cast<int>(a * pulse));
            drawList->AddLine(ImVec2(screenX - size, screenY), ImVec2(screenX + size, screenY), pulseColor, 2.0f);
            drawList->AddLine(ImVec2(screenX, screenY - size), ImVec2(screenX, screenY + size), pulseColor, 2.0f);
            drawList->AddLine(ImVec2(screenX - size * 0.7f, screenY - size * 0.7f), ImVec2(screenX + size * 0.7f, screenY + size * 0.7f), pulseColor, 1.5f);
            drawList->AddLine(ImVec2(screenX - size * 0.7f, screenY + size * 0.7f), ImVec2(screenX + size * 0.7f, screenY - size * 0.7f), pulseColor, 1.5f);
            drawList->AddCircleFilled(screenPos, size * 0.3f, IM_COL32(255, 255, 255, static_cast<int>(a * pulse)), 8);
            break;
        }

        case ProceduralShape::Circle:
        case ProceduralShape::None:
        default:
            // デフォルト：グローエフェクト付き円
            if (a > 50) {
                ImU32 glowColor = IM_COL32(r, g, b, a / 4);
                drawList->AddCircleFilled(screenPos, size * 1.8f, glowColor, 12);
            }
            drawList->AddCircleFilled(screenPos, size, color, 16);
            if (p.color.w > 0.3f) {
                int ha = (std::min)(255, static_cast<int>(p.color.w * 150));
                ImU32 highlight = IM_COL32(255, 255, 255, ha);
                drawList->AddCircleFilled(screenPos, size * 0.4f, highlight, 8);
            }
            break;
        }
    }
    
    // アトラクターの表示（デバッグ用）
    if (selectedEmitter_) {
        auto& config = selectedEmitter_->GetConfig();
        if (config.attractor.enabled) {
            for (const auto& attr : config.attractor.attractors) {
                if (!attr.enabled) continue;
                
                // アトラクター位置を画面座標に変換
                float viewX = attr.position.x * cosCam - attr.position.z * sinCam;
                float viewZ = attr.position.x * sinCam + attr.position.z * cosCam;
                float viewY = attr.position.y * cosPitch - viewZ * sinPitch;
                float finalZ = attr.position.y * sinPitch + viewZ * cosPitch;
                
                float depth = finalZ + previewOrbitDistance_;
                if (depth < 0.5f) depth = 0.5f;
                float depthScale = previewOrbitDistance_ / depth;
                
                float screenX = centerX + viewX * scale * depthScale;
                float screenY = centerY - viewY * scale * depthScale;
                
                // アトラクターを十字で描画
                ImU32 attrColor = attr.strength > 0 ? IM_COL32(100, 200, 255, 200) : IM_COL32(255, 100, 100, 200);
                float attrSize = 8.0f;
                drawList->AddLine(
                    ImVec2(screenX - attrSize, screenY),
                    ImVec2(screenX + attrSize, screenY),
                    attrColor, 2.0f);
                drawList->AddLine(
                    ImVec2(screenX, screenY - attrSize),
                    ImVec2(screenX, screenY + attrSize),
                    attrColor, 2.0f);
                drawList->AddCircle(ImVec2(screenX, screenY), attrSize * 1.5f, attrColor, 12, 1.5f);
            }
        }
        
        // フォースフィールドの可視化
        if (config.forceField.enabled) {
            for (const auto& field : config.forceField.fields) {
                if (!field.enabled) continue;
                if (field.type != ForceFieldType::Vortex) continue;
                
                // 渦巻きの中心を描画
                float viewX = field.position.x * cosCam - field.position.z * sinCam;
                float viewZ = field.position.x * sinCam + field.position.z * cosCam;
                float viewY = field.position.y * cosPitch - viewZ * sinPitch;
                float finalZ = field.position.y * sinPitch + viewZ * cosPitch;
                
                float depth = finalZ + previewOrbitDistance_;
                if (depth < 0.5f) depth = 0.5f;
                float depthScale = previewOrbitDistance_ / depth;
                
                float screenX = centerX + viewX * scale * depthScale;
                float screenY = centerY - viewY * scale * depthScale;
                
                // 渦巻き表示
                ImU32 vortexColor = IM_COL32(150, 255, 150, 100);
                float vortexRadius = field.radius * scale * depthScale;
                drawList->AddCircle(ImVec2(screenX, screenY), vortexRadius, vortexColor, 32, 1.0f);
                
                // 回転方向インジケーター
                float arrowAngle = previewTime_ * field.rotationSpeed * 0.1f;
                float arrowX = screenX + std::cos(arrowAngle) * vortexRadius * 0.8f;
                float arrowY = screenY + std::sin(arrowAngle) * vortexRadius * 0.8f;
                drawList->AddCircleFilled(ImVec2(arrowX, arrowY), 4.0f, IM_COL32(200, 255, 200, 200));
            }
        }
    }
}

void ParticleEditor::CreateDemoPreset() {
    if (!particleSystem_) return;
    
    // 既存エミッターをクリア
    particleSystem_->RemoveAllEmitters();
    previewParticles_.clear();
    previewTime_ = 0.0f;
    previewEmitAccumulator_ = 0.0f;
    
    // デモ用エミッター作成
    EmitterConfig config;
    config.name = "Demo Fire";
    config.duration = 5.0f;
    config.looping = true;
    config.maxParticles = 1000;
    
    // 放出設定
    config.emitRate = 50.0f;
    
    // 形状：コーン
    config.shape.shape = EmitShape::Cone;
    config.shape.coneAngle = 25.0f;
    config.shape.coneRadius = 0.2f;
    
    // ライフタイム
    config.startLifetime = MinMaxCurve::Range(1.0f, 2.0f);
    
    // 速度
    config.startSpeed = MinMaxCurve::Range(2.0f, 4.0f);
    
    // サイズ
    config.startSize = MinMaxCurve::Range(0.3f, 0.6f);
    
    // サイズ変化
    config.sizeOverLifetime.enabled = true;
    config.sizeOverLifetime.size = MinMaxCurve::Constant(1.0f);
    config.sizeOverLifetime.size.mode = MinMaxCurveMode::Curve;
    config.sizeOverLifetime.size.curveMin = AnimationCurve::Linear();
    config.sizeOverLifetime.size.curveMin.GetKeys()[0].value = 0.5f;
    config.sizeOverLifetime.size.curveMin.GetKeys()[1].value = 0.0f;
    
    // カラー（炎のグラデーション）
    config.startColor = MinMaxGradient::Color({ 1.0f, 0.8f, 0.3f, 1.0f });
    
    // カラー変化
    config.colorOverLifetime.enabled = true;
    config.colorOverLifetime.color.mode = MinMaxGradientMode::Gradient;
    config.colorOverLifetime.color.gradientMin = Gradient::Fire();
    
    // エミッター作成
    auto* emitter = particleSystem_->CreateEmitter(config);
    emitter->Play();
    
    selectedEmitter_ = emitter;
    selectedEmitterIndex_ = 0;
    isPlaying_ = true;
    hasUnsavedChanges_ = true;
}

void ParticleEditor::CreateSmokePreset() {
    if (!particleSystem_) return;
    
    particleSystem_->RemoveAllEmitters();
    previewParticles_.clear();
    previewTime_ = 0.0f;
    previewEmitAccumulator_ = 0.0f;
    
    EmitterConfig config;
    config.name = "Demo Smoke";
    config.duration = 5.0f;
    config.looping = true;
    config.maxParticles = 500;
    
    config.emitRate = 15.0f;
    
    config.shape.shape = EmitShape::Circle;
    config.shape.radius = 0.3f;
    
    config.startLifetime = MinMaxCurve::Range(2.0f, 4.0f);
    config.startSpeed = MinMaxCurve::Range(0.5f, 1.5f);
    config.startSize = MinMaxCurve::Range(0.5f, 1.0f);
    
    // サイズ増加
    config.sizeOverLifetime.enabled = true;
    config.sizeOverLifetime.size.mode = MinMaxCurveMode::Curve;
    config.sizeOverLifetime.size.curveMin.GetKeys()[0].value = 1.0f;
    config.sizeOverLifetime.size.curveMin.GetKeys()[1].value = 3.0f;
    
    // グレーの煙
    config.startColor = MinMaxGradient::Color({ 0.5f, 0.5f, 0.5f, 0.6f });
    config.colorOverLifetime.enabled = true;
    config.colorOverLifetime.color.mode = MinMaxGradientMode::Gradient;
    config.colorOverLifetime.color.gradientMin.GetColorKeys().clear();
    config.colorOverLifetime.color.gradientMin.GetColorKeys().push_back({ { 0.5f, 0.5f, 0.5f }, 0.0f });
    config.colorOverLifetime.color.gradientMin.GetColorKeys().push_back({ { 0.3f, 0.3f, 0.3f }, 1.0f });
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().clear();
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 0.0f, 0.0f });
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 0.6f, 0.2f });
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 0.0f, 1.0f });
    
    auto* emitter = particleSystem_->CreateEmitter(config);
    emitter->Play();
    
    selectedEmitter_ = emitter;
    selectedEmitterIndex_ = 0;
    isPlaying_ = true;
    hasUnsavedChanges_ = true;
}

void ParticleEditor::CreateSparkPreset() {
    if (!particleSystem_) return;
    
    particleSystem_->RemoveAllEmitters();
    previewParticles_.clear();
    previewTime_ = 0.0f;
    previewEmitAccumulator_ = 0.0f;
    
    EmitterConfig config;
    config.name = "Demo Sparks";
    config.duration = 5.0f;
    config.looping = true;
    config.maxParticles = 200;
    
    config.emitRate = 30.0f;
    
    config.shape.shape = EmitShape::Sphere;
    config.shape.radius = 0.1f;
    
    config.startLifetime = MinMaxCurve::Range(0.3f, 0.8f);
    config.startSpeed = MinMaxCurve::Range(5.0f, 10.0f);
    config.startSize = MinMaxCurve::Range(0.05f, 0.15f);
    
    // 明るい黄色/オレンジの火花
    config.startColor.mode = MinMaxGradientMode::RandomBetweenColors;
    config.startColor.colorMin = { 1.0f, 0.9f, 0.5f, 1.0f };
    config.startColor.colorMax = { 1.0f, 0.6f, 0.2f, 1.0f };
    
    // フェードアウト
    config.colorOverLifetime.enabled = true;
    config.colorOverLifetime.color.mode = MinMaxGradientMode::Gradient;
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().clear();
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 1.0f, 0.0f });
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 0.0f, 1.0f });
    
    auto* emitter = particleSystem_->CreateEmitter(config);
    emitter->Play();
    
    selectedEmitter_ = emitter;
    selectedEmitterIndex_ = 0;
    isPlaying_ = true;
    hasUnsavedChanges_ = true;
}

void ParticleEditor::CreateAuraPreset() {
    if (!particleSystem_) return;
    
    particleSystem_->RemoveAllEmitters();
    previewParticles_.clear();
    previewTime_ = 0.0f;
    previewEmitAccumulator_ = 0.0f;
    
    EmitterConfig config;
    config.name = "Aura Effect";
    config.duration = 5.0f;
    config.looping = true;
    config.maxParticles = 300;
    
    config.emitRate = 40.0f;
    
    // 球形で外向きに放出
    config.shape.shape = EmitShape::Sphere;
    config.shape.radius = 1.0f;
    
    config.startLifetime = MinMaxCurve::Range(1.0f, 2.0f);
    config.startSpeed = MinMaxCurve::Range(0.5f, 1.5f);
    config.startSize = MinMaxCurve::Range(0.1f, 0.3f);
    
    // 青～紫のオーラ
    config.startColor.mode = MinMaxGradientMode::RandomBetweenColors;
    config.startColor.colorMin = { 0.3f, 0.5f, 1.0f, 0.8f };
    config.startColor.colorMax = { 0.7f, 0.3f, 1.0f, 0.8f };
    
    // サイズ縮小とフェードアウト
    config.sizeOverLifetime.enabled = true;
    config.sizeOverLifetime.size = MinMaxCurve::Constant(0.0f);  // 縮小
    
    config.colorOverLifetime.enabled = true;
    config.colorOverLifetime.color.mode = MinMaxGradientMode::Gradient;
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().clear();
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 0.0f, 0.0f });
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 1.0f, 0.3f });
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 0.0f, 1.0f });
    
    auto* emitter = particleSystem_->CreateEmitter(config);
    emitter->Play();
    
    selectedEmitter_ = emitter;
    selectedEmitterIndex_ = 0;
    isPlaying_ = true;
    hasUnsavedChanges_ = true;
}

void ParticleEditor::CreateExplosionPreset() {
    if (!particleSystem_) return;
    
    particleSystem_->RemoveAllEmitters();
    previewParticles_.clear();
    previewTime_ = 0.0f;
    previewEmitAccumulator_ = 0.0f;
    
    EmitterConfig config;
    config.name = "Explosion";
    config.duration = 0.5f;
    config.looping = true;  // デモ用にループ
    config.maxParticles = 500;
    
    // バーストで一気に放出
    config.emitRate = 0.0f;
    BurstConfig burst;
    burst.time = 0.0f;
    burst.count = 75;  // 固定数
    burst.cycles = 1;
    burst.interval = 0.1f;
    config.bursts.push_back(burst);
    
    config.shape.shape = EmitShape::Sphere;
    config.shape.radius = 0.2f;
    
    config.startLifetime = MinMaxCurve::Range(0.5f, 1.5f);
    config.startSpeed = MinMaxCurve::Range(5.0f, 15.0f);
    config.startSize = MinMaxCurve::Range(0.2f, 0.5f);
    
    // オレンジ～赤の爆発
    config.startColor.mode = MinMaxGradientMode::RandomBetweenColors;
    config.startColor.colorMin = { 1.0f, 0.8f, 0.2f, 1.0f };
    config.startColor.colorMax = { 1.0f, 0.3f, 0.1f, 1.0f };
    
    // 急速にフェードアウト
    config.colorOverLifetime.enabled = true;
    config.colorOverLifetime.color.mode = MinMaxGradientMode::Gradient;
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().clear();
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 1.0f, 0.0f });
    config.colorOverLifetime.color.gradientMin.GetAlphaKeys().push_back({ 0.0f, 0.5f });
    
    auto* emitter = particleSystem_->CreateEmitter(config);
    emitter->Play();
    
    selectedEmitter_ = emitter;
    selectedEmitterIndex_ = 0;
    isPlaying_ = true;
    hasUnsavedChanges_ = true;
}

void ParticleEditor::CreateRainPreset() {
    if (!particleSystem_) return;
    
    particleSystem_->RemoveAllEmitters();
    previewParticles_.clear();
    previewTime_ = 0.0f;
    previewEmitAccumulator_ = 0.0f;
    
    EmitterConfig config;
    config.name = "Rain";
    config.duration = 10.0f;
    config.looping = true;
    config.maxParticles = 1000;
    
    config.emitRate = 200.0f;
    
    // 上から広範囲に
    config.shape.shape = EmitShape::Box;
    config.shape.boxSize = { 10.0f, 0.1f, 10.0f };
    config.shape.position = { 0.0f, 10.0f, 0.0f };
    
    config.startLifetime = MinMaxCurve::Range(1.0f, 2.0f);
    config.startSpeed = MinMaxCurve::Range(8.0f, 12.0f);
    config.startSize = MinMaxCurve::Range(0.02f, 0.05f);
    
    // 青みがかった透明の雨粒
    config.startColor.mode = MinMaxGradientMode::Constant;
    config.startColor.colorMin = { 0.6f, 0.7f, 0.9f, 0.5f };
    config.startColor.colorMax = config.startColor.colorMin;
    
    // 下向きに（Y軸方向の速度を追加）
    config.velocityOverLifetime.enabled = true;
    config.velocityOverLifetime.y = MinMaxCurve::Constant(-5.0f);
    
    auto* emitter = particleSystem_->CreateEmitter(config);
    emitter->Play();
    
    selectedEmitter_ = emitter;
    selectedEmitterIndex_ = 0;
    isPlaying_ = true;
    hasUnsavedChanges_ = true;
}

void ParticleEditor::CreateSnowPreset() {
    if (!particleSystem_) return;
    
    particleSystem_->RemoveAllEmitters();
    previewParticles_.clear();
    previewTime_ = 0.0f;
    previewEmitAccumulator_ = 0.0f;
    
    EmitterConfig config;
    config.name = "Snow";
    config.duration = 10.0f;
    config.looping = true;
    config.maxParticles = 500;
    
    config.emitRate = 50.0f;
    
    // 上から広範囲に
    config.shape.shape = EmitShape::Box;
    config.shape.boxSize = { 8.0f, 0.1f, 8.0f };
    config.shape.position = { 0.0f, 8.0f, 0.0f };
    
    config.startLifetime = MinMaxCurve::Range(4.0f, 8.0f);
    config.startSpeed = MinMaxCurve::Range(0.5f, 1.5f);
    config.startSize = MinMaxCurve::Range(0.05f, 0.15f);
    config.startRotation = MinMaxCurve::Range(0.0f, 360.0f);
    
    // 白い雪
    config.startColor.mode = MinMaxGradientMode::Constant;
    config.startColor.colorMin = { 1.0f, 1.0f, 1.0f, 0.9f };
    config.startColor.colorMax = config.startColor.colorMin;
    
    // ゆっくり下に揺れながら
    config.velocityOverLifetime.enabled = true;
    config.velocityOverLifetime.y = MinMaxCurve::Constant(-0.5f);
    
    // 回転
    config.rotationOverLifetime.enabled = true;
    config.rotationOverLifetime.angularVelocity = MinMaxCurve::Range(-45.0f, 45.0f);
    
    auto* emitter = particleSystem_->CreateEmitter(config);
    emitter->Play();
    
    selectedEmitter_ = emitter;
    selectedEmitterIndex_ = 0;
    isPlaying_ = true;
    hasUnsavedChanges_ = true;
}

void ParticleEditor::RenderPreview() {
    if (!isVisible_ || !use3DPreview_) return;
    if (!previewRenderTexture_ || !particleSystem_ || !previewCamera_) return;
    if (!graphics_) return;

    auto commandList = graphics_->GetCommandList();
    if (!commandList) return;

    // RenderTextureへの描画準備
    auto rtv = previewRenderTexture_->GetRTVHandle();
    auto dsv = previewRenderTexture_->GetDSVHandle();
    auto resource = previewRenderTexture_->GetResource();

    if (!resource) return;

    // リソースバリア：Render Target状態へ
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    commandList->ResourceBarrier(1, &barrier);

    // クリア（暗い青緑の背景）
    const float clearColor[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // レンダーターゲット設定
    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    // ビューポート設定
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(previewWidth_);
    viewport.Height = static_cast<float>(previewHeight_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(previewWidth_), static_cast<LONG>(previewHeight_) };
    commandList->RSSetScissorRects(1, &scissorRect);

    // GPUパーティクルを描画
    particleSystem_->Render(previewCamera_.get());

    // リソースバリア：Shader Resource状態へ戻す
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    commandList->ResourceBarrier(1, &barrier);
}

void ParticleEditor::Render3DPreview() {
    // RenderPreviewのエイリアス（互換性のため）
    RenderPreview();
}


// フォースフィールドセクション
void ParticleEditor::DrawForceFieldSection(EmitterConfig& config) {
    hasUnsavedChanges_ |= ImGui::Checkbox("有効##ForceField", &config.forceField.enabled);
    
    if (!config.forceField.enabled) {
        ImGui::BeginDisabled();
    }
    
    // フォースフィールド追加ボタン
    if (ImGui::Button("+ フォースフィールド追加")) {
        ForceField newField;
        newField.enabled = true;
        config.forceField.fields.push_back(newField);
        hasUnsavedChanges_ = true;
    }
    
    // 各フォースフィールドを描画
    int removeIndex = -1;
    for (size_t i = 0; i < config.forceField.fields.size(); i++) {
        auto& field = config.forceField.fields[i];
        
        ImGui::PushID(static_cast<int>(i));
        
        // ヘッダー
        const char* typeNames[] = { "方向", "放射", "渦巻き", "乱流", "抵抗" };
        char headerLabel[64];
        sprintf_s(headerLabel, "%s [%zu]##field", typeNames[static_cast<int>(field.type)], i);
        
        bool open = ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen);
        
        // 削除ボタン
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        if (ImGui::Button("X##RemoveField")) {
            removeIndex = static_cast<int>(i);
        }
        
        if (open) {
            hasUnsavedChanges_ |= ImGui::Checkbox("有効##FieldEnabled", &field.enabled);
            
            // タイプ選択
            int typeIndex = static_cast<int>(field.type);
            if (ImGui::Combo("タイプ", &typeIndex, "方向\0放射\0渦巻き\0乱流\0抵抗\0")) {
                field.type = static_cast<ForceFieldType>(typeIndex);
                hasUnsavedChanges_ = true;
            }
            
            // 形状選択
            int shapeIndex = static_cast<int>(field.shape);
            if (ImGui::Combo("形状", &shapeIndex, "無限\0球\0ボックス\0円柱\0")) {
                field.shape = static_cast<ForceFieldShape>(shapeIndex);
                hasUnsavedChanges_ = true;
            }
            
            // 位置・サイズ
            hasUnsavedChanges_ |= ImGui::DragFloat3("位置", &field.position.x, 0.1f);
            hasUnsavedChanges_ |= ImGui::DragFloat("半径", &field.radius, 0.1f, 0.1f, 100.0f);
            hasUnsavedChanges_ |= ImGui::DragFloat3("サイズ", &field.size.x, 0.1f, 0.1f, 100.0f);
            
            // 共通パラメータ
            hasUnsavedChanges_ |= ImGui::DragFloat("強さ", &field.strength, 0.1f, -100.0f, 100.0f);
            hasUnsavedChanges_ |= ImGui::DragFloat("減衰", &field.attenuation, 0.01f, 0.0f, 1.0f);
            
            // タイプ別パラメータ
            switch (field.type) {
            case ForceFieldType::Directional:
                hasUnsavedChanges_ |= ImGui::DragFloat3("方向", &field.direction.x, 0.01f, -1.0f, 1.0f);
                break;
                
            case ForceFieldType::Vortex:
                ImGui::Text("--- 渦巻き設定 ---");
                hasUnsavedChanges_ |= ImGui::DragFloat3("回転軸", &field.axis.x, 0.01f, -1.0f, 1.0f);
                hasUnsavedChanges_ |= ImGui::DragFloat("回転速度", &field.rotationSpeed, 0.1f, -50.0f, 50.0f);
                hasUnsavedChanges_ |= ImGui::DragFloat("内向き力", &field.inwardForce, 0.1f, -20.0f, 20.0f);
                hasUnsavedChanges_ |= ImGui::DragFloat("上向き力", &field.upwardForce, 0.1f, -20.0f, 20.0f);
                break;
                
            case ForceFieldType::Turbulence:
                ImGui::Text("--- 乱流設定 ---");
                hasUnsavedChanges_ |= ImGui::DragFloat("周波数", &field.frequency, 0.01f, 0.01f, 10.0f);
                hasUnsavedChanges_ |= ImGui::DragInt("オクターブ", &field.octaves, 1, 1, 8);
                break;
                
            case ForceFieldType::Drag:
                hasUnsavedChanges_ |= ImGui::DragFloat("抵抗係数", &field.dragCoefficient, 0.01f, 0.0f, 1.0f);
                break;
                
            default:
                break;
            }
        }
        
        ImGui::PopID();
    }
    
    // 削除処理
    if (removeIndex >= 0) {
        config.forceField.fields.erase(config.forceField.fields.begin() + removeIndex);
        hasUnsavedChanges_ = true;
    }
    
    if (!config.forceField.enabled) {
        ImGui::EndDisabled();
    }
}

// アトラクターセクション
void ParticleEditor::DrawAttractorSection(EmitterConfig& config) {
    hasUnsavedChanges_ |= ImGui::Checkbox("有効##Attractor", &config.attractor.enabled);
    
    if (!config.attractor.enabled) {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Button("+ アトラクター追加")) {
        Attractor newAttractor;
        newAttractor.enabled = true;
        config.attractor.attractors.push_back(newAttractor);
        hasUnsavedChanges_ = true;
    }
    
    int removeIndex = -1;
    for (size_t i = 0; i < config.attractor.attractors.size(); i++) {
        auto& attr = config.attractor.attractors[i];
        
        ImGui::PushID(static_cast<int>(i));
        
        char headerLabel[64];
        sprintf_s(headerLabel, "アトラクター [%zu]", i);
        
        bool open = ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen);
        
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        if (ImGui::Button("X##RemoveAttr")) {
            removeIndex = static_cast<int>(i);
        }
        
        if (open) {
            hasUnsavedChanges_ |= ImGui::Checkbox("有効##AttrEnabled", &attr.enabled);
            hasUnsavedChanges_ |= ImGui::DragFloat3("位置", &attr.position.x, 0.1f);
            hasUnsavedChanges_ |= ImGui::DragFloat("強さ (正=引力, 負=斥力)", &attr.strength, 0.1f, -100.0f, 100.0f);
            hasUnsavedChanges_ |= ImGui::DragFloat("影響範囲", &attr.radius, 0.1f, 0.1f, 100.0f);
            hasUnsavedChanges_ |= ImGui::DragFloat("デッドゾーン", &attr.deadzone, 0.01f, 0.0f, 10.0f);
            hasUnsavedChanges_ |= ImGui::DragFloat("内側範囲", &attr.innerRadius, 0.01f, 0.0f, 10.0f);
            hasUnsavedChanges_ |= ImGui::Checkbox("接触時に消滅", &attr.killOnContact);
        }
        
        ImGui::PopID();
    }
    
    if (removeIndex >= 0) {
        config.attractor.attractors.erase(config.attractor.attractors.begin() + removeIndex);
        hasUnsavedChanges_ = true;
    }
    
    if (!config.attractor.enabled) {
        ImGui::EndDisabled();
    }
}

// 軌道運動セクション
void ParticleEditor::DrawOrbitalSection(EmitterConfig& config) {
    hasUnsavedChanges_ |= ImGui::Checkbox("有効##Orbital", &config.orbital.enabled);
    
    if (!config.orbital.enabled) {
        ImGui::BeginDisabled();
    }
    
    ImGui::Text("パーティクルを軌道上で回転させます");
    ImGui::Separator();
    
    hasUnsavedChanges_ |= ImGui::DragFloat3("中心点", &config.orbital.center.x, 0.1f);
    hasUnsavedChanges_ |= ImGui::DragFloat3("回転軸", &config.orbital.axis.x, 0.01f, -1.0f, 1.0f);
    
    // 角速度（MinMaxCurve）
    ImGui::Text("角速度 (度/秒)");
    float angVel = config.orbital.angularVelocity.Evaluate(0.5f, 0.5f);
    if (ImGui::DragFloat("角速度##OrbitalAngVel", &angVel, 1.0f, -720.0f, 720.0f)) {
        config.orbital.angularVelocity = MinMaxCurve::Constant(angVel);
        hasUnsavedChanges_ = true;
    }
    
    // 半径方向速度
    ImGui::Text("半径方向速度");
    float radVel = config.orbital.radialVelocity.Evaluate(0.5f, 0.5f);
    if (ImGui::DragFloat("半径速度##OrbitalRadVel", &radVel, 0.1f, -10.0f, 10.0f)) {
        config.orbital.radialVelocity = MinMaxCurve::Constant(radVel);
        hasUnsavedChanges_ = true;
    }
    
    hasUnsavedChanges_ |= ImGui::DragFloat("初期半径", &config.orbital.startRadius, 0.1f, 0.1f, 50.0f);
    hasUnsavedChanges_ |= ImGui::Checkbox("エミッター回転を継承", &config.orbital.inheritEmitterRotation);
    
    if (!config.orbital.enabled) {
        ImGui::EndDisabled();
    }
}

// リボンセクション
void ParticleEditor::DrawRibbonSection(EmitterConfig& config) {
    hasUnsavedChanges_ |= ImGui::Checkbox("有効##Ribbon", &config.ribbon.enabled);
    
    if (!config.ribbon.enabled) {
        ImGui::BeginDisabled();
    }
    
    ImGui::Text("パーティクルの軌跡にリボンを描画します");
    ImGui::Separator();
    
    hasUnsavedChanges_ |= ImGui::DragInt("セグメント数", &config.ribbon.segments, 1, 2, 100);
    hasUnsavedChanges_ |= ImGui::DragFloat("長さ", &config.ribbon.length, 0.1f, 0.1f, 20.0f);
    hasUnsavedChanges_ |= ImGui::DragFloat("幅", &config.ribbon.width, 0.01f, 0.01f, 5.0f);
    hasUnsavedChanges_ |= ImGui::DragFloat("UVリピート", &config.ribbon.uvRepeat, 0.1f, 0.1f, 10.0f);
    hasUnsavedChanges_ |= ImGui::Checkbox("カメラに向ける", &config.ribbon.faceCameraAxis);
    
    // 幅カーブのプレビュー
    ImGui::Text("幅カーブ:");
    ImVec2 curveSize(ImGui::GetContentRegionAvail().x, 60);
    ImGui::PlotLines("##WidthCurve", [](void* data, int idx) {
        auto* curve = static_cast<MinMaxCurve*>(data);
        float t = static_cast<float>(idx) / 50.0f;
        return curve->Evaluate(t, 0.5f);
    }, &config.ribbon.widthOverLength, 50, 0, nullptr, 0.0f, 2.0f, curveSize);
    
    if (!config.ribbon.enabled) {
        ImGui::EndDisabled();
    }
}

// 竜巻プリセット
void ParticleEditor::CreateTornadoPreset() {
    if (!particleSystem_) return;

    // 既存エミッターをクリア
    particleSystem_->RemoveAllEmitters();
    
    // プレビューパーティクルもクリア
    previewParticles_.clear();
    previewEmitAccumulator_ = 0.0f;
    previewTime_ = 0.0f;

    // シンプルなエミッター（フォースフィールドなし）
    EmitterConfig config;
    config.name = "Tornado Main";
    config.duration = 10.0f;
    config.looping = true;
    config.maxParticles = 500;
    config.emitRate = 50.0f;

    config.shape.shape = EmitShape::Cone;
    config.shape.radius = 0.5f;
    config.shape.coneAngle = 5.0f;

    config.startLifetime = MinMaxCurve::Range(2.0f, 4.0f);
    config.startSpeed = MinMaxCurve::Range(0.5f, 1.0f);
    config.startSize = MinMaxCurve::Range(0.1f, 0.3f);
    config.startColor = MinMaxGradient::Color({ 0.7f, 0.7f, 0.7f, 0.8f });

    config.renderMode = RenderMode::Billboard;
    config.blendMode = BlendMode::AlphaBlend;

    selectedEmitter_ = particleSystem_->CreateEmitter(config);
    hasUnsavedChanges_ = true;
}

// 渦巻きエフェクトプリセット
void ParticleEditor::CreateVortexPreset() {
    if (!particleSystem_) return;

    particleSystem_->RemoveAllEmitters();
    
    // プレビューパーティクルもクリア
    previewParticles_.clear();
    previewEmitAccumulator_ = 0.0f;
    previewTime_ = 0.0f;

    EmitterConfig config;
    config.name = "Vortex";
    config.duration = 5.0f;
    config.looping = true;
    config.maxParticles = 1000;
    config.emitRate = 100.0f;

    // 円形状から放出
    config.shape.shape = EmitShape::Circle;
    config.shape.radius = 2.0f;
    config.shape.emitFromEdge = true;

    config.startLifetime = MinMaxCurve::Range(1.5f, 3.0f);
    config.startSpeed = MinMaxCurve::Constant(0.0f);
    config.startSize = MinMaxCurve::Range(0.2f, 0.5f);
    config.startColor = MinMaxGradient::Color({ 0.2f, 0.5f, 1.0f, 1.0f });

    // 中心に向かうアトラクター
    config.attractor.enabled = true;
    Attractor centerPull;
    centerPull.enabled = true;
    centerPull.position = { 0.0f, 0.0f, 0.0f };
    centerPull.strength = 3.0f;
    centerPull.radius = 10.0f;
    centerPull.deadzone = 0.3f;
    centerPull.killOnContact = true;
    config.attractor.attractors.push_back(centerPull);

    // 軌道運動
    config.orbital.enabled = true;
    config.orbital.axis = { 0.0f, 1.0f, 0.0f };
    config.orbital.angularVelocity = MinMaxCurve::Constant(270.0f);

    // 色変化（虹色）
    config.colorOverLifetime.enabled = true;
    Gradient grad;
    grad.AddColorKey({ 0.2f, 0.5f, 1.0f }, 0.0f);
    grad.AddColorKey({ 0.8f, 0.2f, 1.0f }, 0.33f);
    grad.AddColorKey({ 1.0f, 0.2f, 0.5f }, 0.66f);
    grad.AddColorKey({ 1.0f, 1.0f, 1.0f }, 1.0f);
    grad.AddAlphaKey(1.0f, 0.0f);
    grad.AddAlphaKey(0.8f, 0.8f);
    grad.AddAlphaKey(0.0f, 1.0f);
    config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

    config.renderMode = RenderMode::Billboard;
    config.blendMode = BlendMode::Additive;

    selectedEmitter_ = particleSystem_->CreateEmitter(config);
    hasUnsavedChanges_ = true;
}

// 魔法陣プリセット（プロシージャル形状版）
void ParticleEditor::CreateMagicCirclePreset() {
    if (!particleSystem_) return;

    particleSystem_->RemoveAllEmitters();

    // プレビューパーティクルもクリア
    previewParticles_.clear();
    previewEmitAccumulator_ = 0.0f;
    previewTime_ = 0.0f;

    // ========================================
    // 1. 3D魔法陣トリガー（描画しない、魔法陣フラグ用のダミーエミッター）
    // ========================================
    {
        EmitterConfig config;
        config.name = "Magic Circle 3D";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 0;  // パーティクルは生成しない
        config.emitRate = 0.0f;   // 発生させない

        // 3D魔法陣を表示するフラグとして使用
        config.proceduralShape = ProceduralShape::MagicCircle;

        selectedEmitter_ = particleSystem_->CreateEmitter(config);
    }

    // ========================================
    // 2. 外側回転リング
    // ========================================
    {
        EmitterConfig config;
        config.name = "Outer Ring";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 200;
        config.emitRate = 60.0f;

        config.shape.shape = EmitShape::Circle;
        config.shape.radius = 7.0f;
        config.shape.emitFromEdge = true;

        config.startLifetime = MinMaxCurve::Constant(1.5f);
        config.startSpeed = MinMaxCurve::Constant(0.0f);
        config.startSize = MinMaxCurve::Range(0.2f, 0.4f);
        config.startColor = MinMaxGradient::Color({ 0.2f, 0.5f, 1.0f, 1.0f });

        // プロシージャル：きらめき
        config.proceduralShape = ProceduralShape::Sparkle;

        // 軌道運動
        config.orbital.enabled = true;
        config.orbital.axis = { 0.0f, 1.0f, 0.0f };
        config.orbital.angularVelocity = MinMaxCurve::Constant(45.0f);
        config.orbital.startRadius = 7.0f;

        config.colorOverLifetime.enabled = true;
        Gradient grad;
        grad.AddColorKey({ 0.2f, 0.5f, 1.0f }, 0.0f);
        grad.AddColorKey({ 0.5f, 0.3f, 1.0f }, 0.5f);
        grad.AddColorKey({ 0.2f, 0.5f, 1.0f }, 1.0f);
        grad.AddAlphaKey(0.0f, 0.0f);
        grad.AddAlphaKey(1.0f, 0.1f);
        grad.AddAlphaKey(1.0f, 0.9f);
        grad.AddAlphaKey(0.0f, 1.0f);
        config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

        config.renderMode = RenderMode::Billboard;
        config.blendMode = BlendMode::Additive;

        particleSystem_->CreateEmitter(config);
    }

    // ========================================
    // 3. ルーン文字パーティクル
    // ========================================
    {
        EmitterConfig config;
        config.name = "Rune Symbols";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 30;
        config.emitRate = 5.0f;

        config.shape.shape = EmitShape::Circle;
        config.shape.radius = 5.5f;
        config.shape.emitFromEdge = true;

        config.startLifetime = MinMaxCurve::Range(2.0f, 3.0f);
        config.startSpeed = MinMaxCurve::Constant(0.0f);
        config.startSize = MinMaxCurve::Range(0.8f, 1.2f);
        config.startColor = MinMaxGradient::Color({ 1.0f, 0.9f, 0.5f, 1.0f });

        // プロシージャル：ルーン
        config.proceduralShape = ProceduralShape::Rune;

        // ゆっくり軌道を回る
        config.orbital.enabled = true;
        config.orbital.axis = { 0.0f, 1.0f, 0.0f };
        config.orbital.angularVelocity = MinMaxCurve::Constant(-30.0f);
        config.orbital.startRadius = 5.5f;

        // 上下に浮遊
        config.velocityOverLifetime.enabled = true;
        config.velocityOverLifetime.y = MinMaxCurve::Range(-0.3f, 0.3f);

        config.colorOverLifetime.enabled = true;
        Gradient grad;
        grad.AddColorKey({ 1.0f, 0.9f, 0.5f }, 0.0f);
        grad.AddColorKey({ 0.8f, 0.6f, 1.0f }, 0.5f);
        grad.AddColorKey({ 1.0f, 0.9f, 0.5f }, 1.0f);
        grad.AddAlphaKey(0.0f, 0.0f);
        grad.AddAlphaKey(0.8f, 0.2f);
        grad.AddAlphaKey(0.8f, 0.8f);
        grad.AddAlphaKey(0.0f, 1.0f);
        config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

        config.renderMode = RenderMode::Billboard;
        config.blendMode = BlendMode::Additive;

        particleSystem_->CreateEmitter(config);
    }

    // ========================================
    // 4. 六芒星（内側、逆回転）
    // ========================================
    {
        EmitterConfig config;
        config.name = "Inner Star";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 100;
        config.emitRate = 30.0f;

        config.shape.shape = EmitShape::Circle;
        config.shape.radius = 4.0f;
        config.shape.emitFromEdge = true;

        config.startLifetime = MinMaxCurve::Constant(1.2f);
        config.startSpeed = MinMaxCurve::Constant(0.0f);
        config.startSize = MinMaxCurve::Range(0.3f, 0.5f);
        config.startColor = MinMaxGradient::Color({ 0.6f, 0.3f, 1.0f, 1.0f });

        // プロシージャル：星
        config.proceduralShape = ProceduralShape::Star;
        config.proceduralParam1 = 0.5f;  // 内側比率
        config.proceduralParam2 = 6.0f;  // 六芒星

        // 反時計回り
        config.orbital.enabled = true;
        config.orbital.axis = { 0.0f, 1.0f, 0.0f };
        config.orbital.angularVelocity = MinMaxCurve::Constant(-60.0f);
        config.orbital.startRadius = 4.0f;

        config.colorOverLifetime.enabled = true;
        Gradient grad;
        grad.AddColorKey({ 0.6f, 0.3f, 1.0f }, 0.0f);
        grad.AddColorKey({ 1.0f, 0.5f, 0.8f }, 0.5f);
        grad.AddColorKey({ 0.6f, 0.3f, 1.0f }, 1.0f);
        grad.AddAlphaKey(0.0f, 0.0f);
        grad.AddAlphaKey(1.0f, 0.15f);
        grad.AddAlphaKey(1.0f, 0.85f);
        grad.AddAlphaKey(0.0f, 1.0f);
        config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

        config.renderMode = RenderMode::Billboard;
        config.blendMode = BlendMode::Additive;

        particleSystem_->CreateEmitter(config);
    }

    // ========================================
    // 4. 中心のグローコア
    // ========================================
    {
        EmitterConfig config;
        config.name = "Core Glow";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 50;
        config.emitRate = 20.0f;

        config.shape.shape = EmitShape::Sphere;
        config.shape.radius = 0.5f;

        config.startLifetime = MinMaxCurve::Range(0.5f, 1.0f);
        config.startSpeed = MinMaxCurve::Range(0.0f, 0.5f);
        config.startSize = MinMaxCurve::Range(1.5f, 3.0f);
        config.startColor = MinMaxGradient::Color({ 1.0f, 1.0f, 1.0f, 0.8f });

        config.colorOverLifetime.enabled = true;
        Gradient grad;
        grad.AddColorKey({ 1.0f, 1.0f, 1.0f }, 0.0f);
        grad.AddColorKey({ 0.5f, 0.7f, 1.0f }, 0.5f);
        grad.AddColorKey({ 0.3f, 0.5f, 1.0f }, 1.0f);
        grad.AddAlphaKey(0.8f, 0.0f);
        grad.AddAlphaKey(0.5f, 0.5f);
        grad.AddAlphaKey(0.0f, 1.0f);
        config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

        config.sizeOverLifetime.enabled = true;
        config.sizeOverLifetime.size = MinMaxCurve::Range(1.0f, 2.0f);

        config.renderMode = RenderMode::Billboard;
        config.blendMode = BlendMode::Additive;

        particleSystem_->CreateEmitter(config);
    }

    // ========================================
    // 5. 垂直エネルギービーム（上昇）
    // ========================================
    {
        EmitterConfig config;
        config.name = "Energy Beam Up";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 200;
        config.emitRate = 60.0f;

        config.shape.shape = EmitShape::Circle;
        config.shape.radius = 1.5f;
        config.shape.emitFromEdge = false;

        config.startLifetime = MinMaxCurve::Range(1.0f, 1.5f);
        config.startSpeed = MinMaxCurve::Range(4.0f, 7.0f);
        config.startSize = MinMaxCurve::Range(0.15f, 0.4f);
        config.startColor = MinMaxGradient::Color({ 0.4f, 0.7f, 1.0f, 1.0f });

        // 上方向に放出
        config.velocityOverLifetime.enabled = true;
        config.velocityOverLifetime.y = MinMaxCurve::Constant(5.0f);

        config.colorOverLifetime.enabled = true;
        Gradient grad;
        grad.AddColorKey({ 1.0f, 1.0f, 1.0f }, 0.0f);
        grad.AddColorKey({ 0.4f, 0.7f, 1.0f }, 0.3f);
        grad.AddColorKey({ 0.2f, 0.4f, 1.0f }, 1.0f);
        grad.AddAlphaKey(1.0f, 0.0f);
        grad.AddAlphaKey(0.8f, 0.3f);
        grad.AddAlphaKey(0.0f, 1.0f);
        config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

        config.sizeOverLifetime.enabled = true;
        config.sizeOverLifetime.size = MinMaxCurve::Range(1.0f, 0.3f);

        config.renderMode = RenderMode::Billboard;
        config.blendMode = BlendMode::Additive;

        particleSystem_->CreateEmitter(config);
    }

    // ========================================
    // 6. スパーク・きらめきパーティクル
    // ========================================
    {
        EmitterConfig config;
        config.name = "Sparkles";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 150;
        config.emitRate = 40.0f;

        config.shape.shape = EmitShape::Circle;
        config.shape.radius = 6.0f;
        config.shape.emitFromEdge = false;

        config.startLifetime = MinMaxCurve::Range(0.3f, 0.8f);
        config.startSpeed = MinMaxCurve::Range(2.0f, 5.0f);
        config.startSize = MinMaxCurve::Range(0.1f, 0.25f);
        config.startColor = MinMaxGradient::Color({ 1.0f, 1.0f, 1.0f, 1.0f });

        // ランダムな方向に飛び散る
        config.velocityOverLifetime.enabled = true;
        config.velocityOverLifetime.x = MinMaxCurve::Range(-2.0f, 2.0f);
        config.velocityOverLifetime.y = MinMaxCurve::Range(1.0f, 4.0f);
        config.velocityOverLifetime.z = MinMaxCurve::Range(-2.0f, 2.0f);

        config.colorOverLifetime.enabled = true;
        Gradient grad;
        grad.AddColorKey({ 1.0f, 1.0f, 1.0f }, 0.0f);
        grad.AddColorKey({ 0.8f, 0.9f, 1.0f }, 0.5f);
        grad.AddColorKey({ 0.5f, 0.7f, 1.0f }, 1.0f);
        grad.AddAlphaKey(1.0f, 0.0f);
        grad.AddAlphaKey(0.5f, 0.5f);
        grad.AddAlphaKey(0.0f, 1.0f);
        config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

        config.sizeOverLifetime.enabled = true;
        config.sizeOverLifetime.size = MinMaxCurve::Range(1.0f, 0.0f);

        config.renderMode = RenderMode::Billboard;
        config.blendMode = BlendMode::Additive;

        particleSystem_->CreateEmitter(config);
    }

    // ========================================
    // 7. 床面のグロー効果
    // ========================================
    {
        EmitterConfig config;
        config.name = "Ground Glow";
        config.duration = 10.0f;
        config.looping = true;
        config.maxParticles = 30;
        config.emitRate = 8.0f;

        config.shape.shape = EmitShape::Circle;
        config.shape.radius = 4.0f;
        config.shape.emitFromEdge = false;

        config.startLifetime = MinMaxCurve::Range(1.5f, 2.5f);
        config.startSpeed = MinMaxCurve::Constant(0.0f);
        config.startSize = MinMaxCurve::Range(5.0f, 10.0f);
        config.startColor = MinMaxGradient::Color({ 0.2f, 0.3f, 0.8f, 0.3f });

        config.colorOverLifetime.enabled = true;
        Gradient grad;
        grad.AddColorKey({ 0.2f, 0.3f, 0.8f }, 0.0f);
        grad.AddColorKey({ 0.4f, 0.2f, 0.9f }, 0.5f);
        grad.AddColorKey({ 0.2f, 0.3f, 0.8f }, 1.0f);
        grad.AddAlphaKey(0.0f, 0.0f);
        grad.AddAlphaKey(0.3f, 0.3f);
        grad.AddAlphaKey(0.3f, 0.7f);
        grad.AddAlphaKey(0.0f, 1.0f);
        config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

        config.sizeOverLifetime.enabled = true;
        config.sizeOverLifetime.size = MinMaxCurve::Range(0.8f, 1.2f);

        config.renderMode = RenderMode::Billboard;
        config.blendMode = BlendMode::Additive;

        particleSystem_->CreateEmitter(config);
    }

    hasUnsavedChanges_ = true;
}

// 剣の軌跡プリセット
void ParticleEditor::CreateBladeTrailPreset() {
    if (!particleSystem_) return;

    particleSystem_->RemoveAllEmitters();
    
    // プレビューパーティクルもクリア
    previewParticles_.clear();
    previewEmitAccumulator_ = 0.0f;
    previewTime_ = 0.0f;

    EmitterConfig config;
    config.name = "Blade Trail";
    config.duration = 5.0f;
    config.looping = true;
    config.maxParticles = 500;
    config.emitRate = 100.0f;

    // エッジ（線）から放出
    config.shape.shape = EmitShape::Edge;
    config.shape.radius = 1.5f;  // 刃の長さ

    config.startLifetime = MinMaxCurve::Range(0.3f, 0.5f);
    config.startSpeed = MinMaxCurve::Constant(0.0f);
    config.startSize = MinMaxCurve::Range(0.1f, 0.3f);
    config.startColor = MinMaxGradient::Color({ 0.8f, 0.9f, 1.0f, 1.0f });

    // リボン有効
    config.ribbon.enabled = true;
    config.ribbon.segments = 30;
    config.ribbon.length = 1.0f;
    config.ribbon.width = 0.3f;
    config.ribbon.faceCameraAxis = true;
    config.ribbon.widthOverLength = MinMaxCurve::Constant(1.0f);

    // トレイルも有効
    config.trail.enabled = true;
    config.trail.lifetime = 0.3f;
    config.trail.minVertexDistance = 0.05f;
    config.trail.widthMultiplier = 0.5f;
    config.trail.inheritParticleColor = true;

    // サイズ変化
    config.sizeOverLifetime.enabled = true;
    config.sizeOverLifetime.size = MinMaxCurve::Range(0.5f, 1.0f);

    // 色変化
    config.colorOverLifetime.enabled = true;
    Gradient grad;
    grad.AddColorKey({ 1.0f, 1.0f, 1.0f }, 0.0f);
    grad.AddColorKey({ 0.5f, 0.7f, 1.0f }, 0.5f);
    grad.AddColorKey({ 0.2f, 0.3f, 0.8f }, 1.0f);
    grad.AddAlphaKey(1.0f, 0.0f);
    grad.AddAlphaKey(0.5f, 0.7f);
    grad.AddAlphaKey(0.0f, 1.0f);
    config.colorOverLifetime.color = MinMaxGradient::FromGradient(grad);

    config.renderMode = RenderMode::Trail;
    config.blendMode = BlendMode::Additive;

    selectedEmitter_ = particleSystem_->CreateEmitter(config);
    hasUnsavedChanges_ = true;
}

} // namespace UnoEngine
