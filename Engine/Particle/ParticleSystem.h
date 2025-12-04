#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "../Graphics/D3D12Common.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Shader.h"
#include "ParticleData.h"
#include "ParticleEmitter.h"
#include <vector>
#include <memory>
#include <string>

namespace UnoEngine {

class Camera;
class Texture2D;

// パーティクルシステム設定
struct ParticleSystemConfig {
    uint32 maxParticles = 100000;   // 最大パーティクル数
    bool enableCollision = true;    // コリジョン有効
    bool enableSorting = false;     // ソート有効
};

// GPU用エミッターパラメータ（シェーダーと一致）
struct alignas(256) GPUEmitterParams {
    Float3 position;
    float emitRate;
    Float3 minVelocity;
    float deltaTime;
    Float3 maxVelocity;
    float time;
    float minLifetime;
    float maxLifetime;
    float minSize;
    float maxSize;
    Float4 startColor;
    Float3 gravity;
    float drag;
    uint32 emitterID;
    uint32 maxParticles;
    uint32 emitShape;
    uint32 flags;
    float shapeRadius;
    float coneAngle;
    Float2 shapePadding;
};

// GPU用システム定数バッファ
struct alignas(256) ParticleSystemCB {
    Float4x4 viewMatrix;
    Float4x4 projMatrix;
    Float4x4 viewProjMatrix;
    Float4x4 invViewMatrix;
    Float3 cameraPosition;
    float totalTime;
    Float3 cameraRight;
    float deltaTime;
    Float3 cameraUp;
    uint32 frameIndex;
};

// GPU用更新パラメータ
struct alignas(256) ParticleUpdateCB {
    Float3 gravity;
    float drag;
    Float4x4 viewProjMatrix_Collision;
    Float4x4 invViewProjMatrix_Collision;
    Float2 screenSize;
    uint32 aliveCountIn;
    uint32 collisionEnabled;
    float collisionBounce;
    float collisionLifetimeLoss;
    Float2 updatePadding;
};

// GPU用レンダリングパラメータ
struct alignas(256) ParticleRenderCB {
    uint32 useTexture;
    uint32 blendMode;
    float softParticleScale;
    uint32 proceduralShape;     // プロシージャル形状タイプ
    float proceduralParam1;     // 形状パラメータ1
    float proceduralParam2;     // 形状パラメータ2
    float totalTime;            // アニメーション用時間
    float padding;
};

class ParticleSystem : public NonCopyable {
public:
    ParticleSystem();
    ~ParticleSystem();

    // 初期化
    void Initialize(GraphicsDevice* graphics, const ParticleSystemConfig& config = {});
    void Shutdown();

    // エミッター管理
    ParticleEmitter* CreateEmitter(const std::string& name = "Emitter");
    ParticleEmitter* CreateEmitter(const EmitterConfig& config);
    void RemoveEmitter(ParticleEmitter* emitter);
    void RemoveAllEmitters();
    ParticleEmitter* GetEmitter(const std::string& name);
    ParticleEmitter* GetEmitter(size_t index);
    size_t GetEmitterCount() const { return emitters_.size(); }

    // 更新・描画
    void Update(float deltaTime);
    void Render(Camera* camera, ID3D12Resource* depthBuffer = nullptr);

    // 全体制御
    void Play();
    void Pause();
    void Stop();
    void Restart();

    // 設定
    void SetGravity(const Float3& gravity) { gravity_ = gravity; }
    const Float3& GetGravity() const { return gravity_; }
    void SetDrag(float drag) { drag_ = drag; }
    float GetDrag() const { return drag_; }

    // 統計
    uint32 GetAliveParticleCount() const { return aliveParticleCount_; }
    uint32 GetMaxParticles() const { return config_.maxParticles; }

    // テクスチャ設定
    void SetDefaultTexture(Texture2D* texture) { defaultTexture_ = texture; }

private:
    void CreateGPUResources();
    void CreateComputePipelines();
    void CreateRenderPipeline();
    void CreateRootSignatures();

    void ResetCounters();
    void EmitParticles(uint32 emitCount, const GPUEmitterParams& emitterParams);
    void UpdateParticles();
    void BuildIndirectArgs();

    void UpdateSystemConstantBuffer(Camera* camera, float deltaTime);

private:
    GraphicsDevice* graphics_ = nullptr;
    ParticleSystemConfig config_;

    // エミッター
    std::vector<std::unique_ptr<ParticleEmitter>> emitters_;

    // GPU バッファ
    ComPtr<ID3D12Resource> particlePool_;           // パーティクルデータプール
    ComPtr<ID3D12Resource> deadList_;               // 未使用スロットリスト
    ComPtr<ID3D12Resource> aliveListA_;             // 生存パーティクルリストA
    ComPtr<ID3D12Resource> aliveListB_;             // 生存パーティクルリストB（ダブルバッファ）
    ComPtr<ID3D12Resource> counterBuffer_;          // カウンター
    ComPtr<ID3D12Resource> indirectArgsBuffer_;     // Indirect描画引数
    ComPtr<ID3D12Resource> counterReadbackBuffer_;  // CPU読み取り用

    // 定数バッファ
    ComPtr<ID3D12Resource> systemCB_;
    ComPtr<ID3D12Resource> emitterCB_;
    ComPtr<ID3D12Resource> updateCB_;
    ComPtr<ID3D12Resource> renderCB_;

    // Compute パイプライン
    ComPtr<ID3D12RootSignature> computeRootSignature_;
    ComPtr<ID3D12PipelineState> emitPSO_;
    ComPtr<ID3D12PipelineState> updatePSO_;
    ComPtr<ID3D12PipelineState> buildArgsPSO_;

    // Render パイプライン
    ComPtr<ID3D12RootSignature> renderRootSignature_;
    ComPtr<ID3D12PipelineState> renderPSO_Additive_;
    ComPtr<ID3D12PipelineState> renderPSO_AlphaBlend_;
    ComPtr<ID3D12PipelineState> renderPSO_Multiply_;

    // Command Signature（ExecuteIndirect用）
    ComPtr<ID3D12CommandSignature> commandSignature_;

    // デスクリプタヒープインデックス
    uint32 particlePoolSRVIndex_ = 0;
    uint32 particlePoolUAVIndex_ = 0;
    uint32 deadListUAVIndex_ = 0;
    uint32 aliveListASRVIndex_ = 0;
    uint32 aliveListAUAVIndex_ = 0;
    uint32 aliveListBSRVIndex_ = 0;
    uint32 aliveListBUAVIndex_ = 0;
    uint32 counterUAVIndex_ = 0;
    uint32 indirectArgsUAVIndex_ = 0;

    // シェーダー
    Shader emitCS_;
    Shader updateCS_;
    Shader buildArgsCS_;
    Shader billboardVS_;
    Shader particlePS_;

    // 状態
    Float3 gravity_ = { 0.0f, -9.8f, 0.0f };
    float drag_ = 0.0f;
    float totalTime_ = 0.0f;
    uint32 frameIndex_ = 0;
    uint32 aliveParticleCount_ = 0;
    bool useAliveListA_ = true;  // ダブルバッファの切り替え

    // テクスチャ
    Texture2D* defaultTexture_ = nullptr;
};

} // namespace UnoEngine
