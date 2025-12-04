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
    
    // 3Dエフェクト用セクション
    void DrawForceFieldSection(EmitterConfig& config);
    void DrawAttractorSection(EmitterConfig& config);
    void DrawOrbitalSection(EmitterConfig& config);
    void DrawRibbonSection(EmitterConfig& config);

    // ファイル操作
    void NewEffect();
    void OpenEffect();
    void SaveEffect();
    void SaveEffectAs();

    // デモプリセット作成
    void CreateDemoPreset();
    void CreateSmokePreset();
    void CreateSparkPreset();
    void CreateAuraPreset();
    void CreateExplosionPreset();
    void CreateRainPreset();
    void CreateSnowPreset();
    void CreateTornadoPreset();
    void CreateVortexPreset();
    void CreateMagicCirclePreset();
    void CreateBladeTrailPreset();

private:
    void UpdatePreviewCamera();
    void UpdatePreviewParticles(float deltaTime);
    void EmitPreviewParticles(int count);
    void DrawPreviewParticles(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    void Render3DPreview();
    void HandlePreviewInput(const ImVec2& canvasPos, const ImVec2& canvasSize);
    void DrawGrid(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    
    GraphicsDevice* graphics_ = nullptr;
    ParticleSystem* particleSystem_ = nullptr;
    ParticleEmitter* selectedEmitter_ = nullptr;

    // プレビュー用カメラ
    std::unique_ptr<Camera> previewCamera_;
    std::unique_ptr<RenderTexture> previewRenderTexture_;
    
    // オービットカメラパラメータ
    float previewOrbitAngle_ = 0.0f;         // 水平回転角（ラジアン）
    float previewOrbitPitch_ = 1.0f;         // 垂直角（ラジアン）- 魔法陣を斜め上から見る
    float previewOrbitDistance_ = 12.0f;     // カメラ距離
    Float3 previewOrbitTarget_ = { 0.0f, 1.0f, 0.0f };  // 注視点（少し上）
    
    // カメラ操作状態
    bool isOrbitDragging_ = false;
    bool isPanDragging_ = false;
    ImVec2 lastMousePos_ = { 0.0f, 0.0f };
    
    // プレビューサイズ
    uint32 previewWidth_ = 600;
    uint32 previewHeight_ = 500;

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
    bool use3DPreview_ = false;  // 3Dプレビュー使用フラグ（GPUレンダリング問題のため一時的にfalse）
    bool showGrid_ = true;      // グリッド表示
    bool showAxis_ = true;      // 軸表示
    
    // CPUプレビュー用パーティクル（フォールバック用）
    std::vector<PreviewParticle> previewParticles_;
    float previewEmitAccumulator_ = 0.0f;
    float previewTime_ = 0.0f;
    std::mt19937 previewRng_;
    std::uniform_real_distribution<float> previewDist_{ 0.0f, 1.0f };
    static constexpr int MAX_PREVIEW_PARTICLES = 500;
};

} // namespace UnoEngine
