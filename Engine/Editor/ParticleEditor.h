#pragma once

#include "../Core/Types.h"
#include "../Particle/ParticleSystem.h"
#include "../Particle/ParticleEmitter.h"
#include "CurveEditor.h"
#include <string>
#include <memory>
#include <vector>
#include <random>

namespace UnoEngine {

class GraphicsDevice;
class Camera;
class RenderTexture;

// CPUプレビュー用パーティクル
struct PreviewParticle {
    Float3 position;
    Float3 velocity;
    Float4 color;
    float size;
    float rotation;
    float lifetime;
    float maxLifetime;
    float random;  // パーティクル固有の乱数
    bool alive;
};

// パーティクルエディターウィンドウ
class ParticleEditor {
public:
    ParticleEditor();
    ~ParticleEditor();

    // 初期化
    void Initialize(GraphicsDevice* graphics, ParticleSystem* particleSystem);

    // UI描画
    void Draw();
    
    // プレビュー描画（Application::OnRenderから呼び出す）
    void RenderPreview();
    
    // プレビュー更新（Application::MainLoopから呼び出す）
    void UpdatePreview(float deltaTime);

    // 表示切り替え
    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsVisible() const { return isVisible_; }
    void ToggleVisible() { isVisible_ = !isVisible_; }

    // 現在のエミッター取得
    ParticleEmitter* GetSelectedEmitter() { return selectedEmitter_; }

private:
    // UI セクション
    void DrawMenuBar();
    void DrawToolbar();
    void DrawEmitterList();
    void DrawEmitterProperties();
    void DrawPreviewWindow();

    // プロパティセクション
    void DrawEmissionSection(EmitterConfig& config);
    void DrawShapeSection(EmitterConfig& config);
    void DrawVelocitySection(EmitterConfig& config);
    void DrawColorSection(EmitterConfig& config);
    void DrawSizeSection(EmitterConfig& config);
    void DrawRotationSection(EmitterConfig& config);
    void DrawCollisionSection(EmitterConfig& config);
    void DrawRenderingSection(EmitterConfig& config);
    void DrawSubEmitterSection(EmitterConfig& config);

    // ファイル操作
    void NewEffect();
    void OpenEffect();
    void SaveEffect();
    void SaveEffectAs();

    // デモプリセット作成
    void CreateDemoPreset();
    void CreateSmokePreset();
    void CreateSparkPreset();

private:
    void UpdatePreviewCamera();
    void UpdatePreviewParticles(float deltaTime);
    void EmitPreviewParticles(int count);
    void DrawPreviewParticles(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    
    GraphicsDevice* graphics_ = nullptr;
    ParticleSystem* particleSystem_ = nullptr;
    ParticleEmitter* selectedEmitter_ = nullptr;

    // プレビュー用
    std::unique_ptr<Camera> previewCamera_;
    std::unique_ptr<RenderTexture> previewRenderTexture_;
    float previewOrbitAngle_ = 0.0f;
    float previewOrbitDistance_ = 5.0f;
    float previewOrbitHeight_ = 2.0f;

    // 状態
    bool isVisible_ = false;
    bool isPlaying_ = true;
    float playbackSpeed_ = 1.0f;
    std::string currentFilePath_;
    bool hasUnsavedChanges_ = false;

    // UI状態
    int selectedEmitterIndex_ = 0;
    int selectedTabIndex_ = 0;
    bool showPreview_ = true;
    bool autoRotatePreview_ = false;
    
    // CPUプレビュー用パーティクル
    std::vector<PreviewParticle> previewParticles_;
    float previewEmitAccumulator_ = 0.0f;
    float previewTime_ = 0.0f;
    std::mt19937 previewRng_;
    std::uniform_real_distribution<float> previewDist_{ 0.0f, 1.0f };
    static constexpr int MAX_PREVIEW_PARTICLES = 500;
};

} // namespace UnoEngine
