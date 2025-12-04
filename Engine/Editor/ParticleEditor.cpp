// UTF-8 BOM: This file should be saved with UTF-8 encoding
#include "pch.h"
#include "ParticleEditor.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/RenderTexture.h"
#include "../Core/Camera.h"
#include "../Graphics/d3dx12.h"
#include <imgui.h>
#include <cmath>

namespace UnoEngine {

ParticleEditor::ParticleEditor() = default;
ParticleEditor::~ParticleEditor() = default;

void ParticleEditor::Initialize(GraphicsDevice* graphics, ParticleSystem* particleSystem) {
    graphics_ = graphics;
    particleSystem_ = particleSystem;

    // プレビューカメラ作成
    previewCamera_ = std::make_unique<Camera>();
    previewCamera_->SetPerspective(45.0f * 3.14159f / 180.0f, 1.0f, 0.1f, 100.0f);
    
    // 初期カメラ位置
    previewCamera_->SetPosition(Vector3(0.0f, previewOrbitHeight_, -previewOrbitDistance_));
    previewCamera_->SetTarget(Vector3(0.0f, 0.0f, 0.0f));

    // プレビュー用レンダーテクスチャ作成
    previewRenderTexture_ = std::make_unique<RenderTexture>();
    uint32 srvIndex = graphics_->AllocateSRVIndex();
    previewRenderTexture_->Create(graphics_, 400, 300, srvIndex);
}

void ParticleEditor::Draw() {
    if (!isVisible_) return;

    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("パーティクルエディター###ParticleEditor", &isVisible_, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    DrawMenuBar();
    DrawToolbar();

    // 左右分割レイアウト
    ImGui::Columns(2, "particle_editor_columns");

    // 左側: プレビュー + エミッターリスト
    if (showPreview_) {
        DrawPreviewWindow();
    }
    DrawEmitterList();

    ImGui::NextColumn();

    // 右側: プロパティ
    DrawEmitterProperties();

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
            if (ImGui::MenuItem("保存", "Ctrl+S")) {
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
            if (ImGui::MenuItem("元に戻す", "Ctrl+Z")) {
                // TODO: Undo
            }
            if (ImGui::MenuItem("やり直す", "Ctrl+Y")) {
                // TODO: Redo
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("表示")) {
            ImGui::MenuItem("プレビュー", nullptr, &showPreview_);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("プリセット")) {
            if (ImGui::MenuItem("炎エフェクト")) {
                CreateDemoPreset();
            }
            if (ImGui::MenuItem("煙エフェクト")) {
                CreateSmokePreset();
            }
            if (ImGui::MenuItem("火花エフェクト")) {
                CreateSparkPreset();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void ParticleEditor::DrawToolbar() {
    // 再生コントロール
    if (isPlaying_) {
        if (ImGui::Button("一時停止")) {
            isPlaying_ = false;
            if (particleSystem_) particleSystem_->Pause();
        }
    } else {
        if (ImGui::Button("再生")) {
            isPlaying_ = true;
            if (particleSystem_) particleSystem_->Play();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("リスタート")) {
        if (particleSystem_) particleSystem_->Restart();
    }

    ImGui::SameLine();
    if (ImGui::Button("停止")) {
        isPlaying_ = false;
        if (particleSystem_) particleSystem_->Stop();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("速度", &playbackSpeed_, 0.1f, 2.0f);

    // 統計表示
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    if (particleSystem_) {
        ImGui::Text("パーティクル数: %u / %u",
            particleSystem_->GetAliveParticleCount(),
            particleSystem_->GetMaxParticles());
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
    ImGui::BeginChild("Preview", ImVec2(0, 300), true);
    
    // ヘッダー行：タイトルとパーティクル数
    int aliveCount = 0;
    for (const auto& p : previewParticles_) {
        if (p.alive) aliveCount++;
    }
    ImGui::Text("プレビュー (%d particles)", aliveCount);
    ImGui::Separator();

    // プレビューキャンバス
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y -= 30;  // コントロール用スペース
    
    if (canvasSize.x < 50) canvasSize.x = 50;
    if (canvasSize.y < 50) canvasSize.y = 50;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    
    // 背景（暗いグラデーション）
    drawList->AddRectFilledMultiColor(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(20, 20, 30, 255),
        IM_COL32(20, 20, 30, 255),
        IM_COL32(40, 40, 50, 255),
        IM_COL32(40, 40, 50, 255)
    );
    
    // グリッド線
    ImU32 gridColor = IM_COL32(60, 60, 70, 100);
    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.7f;  // 少し下寄り
    drawList->AddLine(ImVec2(canvasPos.x, centerY), ImVec2(canvasPos.x + canvasSize.x, centerY), gridColor);
    drawList->AddLine(ImVec2(centerX, canvasPos.y), ImVec2(centerX, canvasPos.y + canvasSize.y), gridColor);
    
    // パーティクル描画
    DrawPreviewParticles(drawList, canvasPos, canvasSize);
    
    // 枠線
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(80, 80, 90, 255));

    ImGui::Dummy(canvasSize);

    // コントロール
    ImGui::Checkbox("自動回転", &autoRotatePreview_);

    ImGui::EndChild();
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

    float x = std::cos(previewOrbitAngle_) * previewOrbitDistance_;
    float z = std::sin(previewOrbitAngle_) * previewOrbitDistance_;

    previewCamera_->SetPosition(Vector3(x, previewOrbitHeight_, z));
    previewCamera_->SetTarget(Vector3(0.0f, 0.0f, 0.0f));
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
    Float3 gravity = { 0.0f, -9.8f, 0.0f };
    
    for (auto& p : previewParticles_) {
        if (!p.alive) continue;
        
        p.lifetime += deltaTime;
        if (p.lifetime >= p.maxLifetime) {
            p.alive = false;
            continue;
        }
        
        // 物理更新
        p.velocity.x += gravity.x * deltaTime;
        p.velocity.y += gravity.y * deltaTime;
        p.velocity.z += gravity.z * deltaTime;
        
        p.position.x += p.velocity.x * deltaTime;
        p.position.y += p.velocity.y * deltaTime;
        p.position.z += p.velocity.z * deltaTime;
        
        // ライフタイム進行率
        float t = p.lifetime / p.maxLifetime;
        
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

void ParticleEditor::DrawPreviewParticles(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    if (previewParticles_.empty()) return;
    
    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.7f;
    float scale = canvasSize.y * 0.08f;  // ワールド座標からスクリーン座標へのスケール
    
    // カメラ方向（簡易的なビルボード）
    float camAngle = previewOrbitAngle_;
    
    for (const auto& p : previewParticles_) {
        if (!p.alive) continue;
        
        // 3D → 2D 変換（簡易的な投影）
        float viewX = p.position.x * std::cos(camAngle) - p.position.z * std::sin(camAngle);
        float viewZ = p.position.x * std::sin(camAngle) + p.position.z * std::cos(camAngle);
        
        // 深度によるスケール（遠近感）
        float depth = viewZ + previewOrbitDistance_;
        if (depth < 0.5f) depth = 0.5f;
        float depthScale = previewOrbitDistance_ / depth;
        
        float screenX = centerX + viewX * scale * depthScale;
        float screenY = centerY - p.position.y * scale * depthScale;  // Y軸反転
        
        // 画面外チェック
        if (screenX < canvasPos.x - 20 || screenX > canvasPos.x + canvasSize.x + 20 ||
            screenY < canvasPos.y - 20 || screenY > canvasPos.y + canvasSize.y + 20) {
            continue;
        }
        
        // サイズ
        float size = p.size * scale * 0.5f * depthScale;
        if (size < 1.0f) size = 1.0f;
        if (size > 50.0f) size = 50.0f;
        
        // カラー
        ImU32 color = IM_COL32(
            static_cast<int>(p.color.x * 255),
            static_cast<int>(p.color.y * 255),
            static_cast<int>(p.color.z * 255),
            static_cast<int>(p.color.w * 255)
        );
        
        // 丸いパーティクル描画
        drawList->AddCircleFilled(ImVec2(screenX, screenY), size, color);
        
        // 加算ブレンド風のハイライト
        if (p.color.w > 0.5f) {
            ImU32 highlight = IM_COL32(255, 255, 255, static_cast<int>(p.color.w * 50));
            drawList->AddCircleFilled(ImVec2(screenX, screenY), size * 0.5f, highlight);
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

void ParticleEditor::RenderPreview() {
    // CPUプレビューを使用するため、GPU描画は不要
}

} // namespace UnoEngine
