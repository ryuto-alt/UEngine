#pragma once

#include "GrassSystem.h"
#include "GrassPipeline.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/DynamicConstantBuffer.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/ShadowMap.h"
#include "../Rendering/LightManager.h"
#include "../Rendering/RenderView.h"
#include "../Math/MathCommon.h"
#include <string>

namespace UnoEngine {

// 草専用の定数バッファ（Godot風シェーダー対応）
struct alignas(256) GrassTransformCB {
    // ===== 変換行列 (448 bytes) =====
    Float4x4 view;
    Float4x4 projection;
    Float4x4 lightViewProj;
    Float4x4 spotLightViewProj[4];

    // ===== カメラ & 時間 (16 bytes) =====
    Float3   cameraPos;          float time;

    // ===== カラーパラメータ (64 bytes) =====
    Float3   bottomColor;        float windSpeed;
    Float3   topColor;           float windDis;
    Float3   colorVar1;          float noiseStrength;
    Float3   colorVar2;          float displaceStrength;

    // ===== 風影 & インタラクション (48 bytes) =====
    Float3   windShadowColor;    float windShadowStrength;
    Float3   interactingObjPos;  float flattenRadius;
    float    flattenStrength;    float flattenFloor;
    float    noise1Scale;        float noise2Scale;

    // ===== ノイズ & スケール (48 bytes) =====
    float    windNoiseScale;     float windNoisePanSpeedX;
    float    windNoisePanSpeedY; float noiseFloor;
    float    windNoiseScaleStrength; float combinedNoiseMinScale;
    float    combinedNoiseMaxScale;  float invertCombinedNoise;
    float    windShadowDispThreshold; float windShadowSmoothing;
    float    flattenShadowStrength;   float baseWidth;

    // ===== 最終行 (16 bytes) =====
    float    baseHeight;         float useGodotShading;
    float    cbPadding[2];
};

// 草の描画を管理
class GrassRenderer {
public:
    GrassRenderer() = default;
    ~GrassRenderer() = default;

    void Initialize(GraphicsDevice* graphics);

    // 草を描画（メインパス内で呼ぶ）
    void Render(const RenderView& view,
                const Matrix4x4& lightViewProj,
                const Matrix4x4 spotLightViewProjs[4],
                int spotShadowCount,
                D3D12_GPU_VIRTUAL_ADDRESS lightCBAddr,
                const ShadowMap& shadowMap,
                const ShadowMap spotShadowMaps[4],
                GrassSystem& grassSystem);

    // 時間更新
    void Update(float deltaTime) { totalTime_ += deltaTime; }

    // ===== カラーパラメータ =====
    void SetBottomColor(float r, float g, float b) { bottomColor_ = { r, g, b }; }
    void SetTopColor(float r, float g, float b) { topColor_ = { r, g, b }; }
    void SetColorVar1(float r, float g, float b) { colorVar1_ = { r, g, b }; }
    void SetColorVar2(float r, float g, float b) { colorVar2_ = { r, g, b }; }
    Float3 GetBottomColor() const { return bottomColor_; }
    Float3 GetTopColor() const { return topColor_; }
    Float3 GetColorVar1() const { return colorVar1_; }
    Float3 GetColorVar2() const { return colorVar2_; }

    // ===== 風パラメータ =====
    void SetWindSpeed(float v) { windSpeed_ = v; }
    void SetWindDis(float v) { windDis_ = v; }
    void SetNoiseStrength(float v) { noiseStrength_ = v; }
    void SetDisplaceStrength(float v) { displaceStrength_ = v; }
    void SetWindNoiseScale(float v) { windNoiseScale_ = v; }
    void SetWindNoisePanSpeed(float x, float y) { windNoisePanSpeedX_ = x; windNoisePanSpeedY_ = y; }
    void SetNoiseFloor(float v) { noiseFloor_ = v; }
    void SetWindNoiseScaleStrength(float v) { windNoiseScaleStrength_ = v; }
    float GetWindSpeed() const { return windSpeed_; }
    float GetWindDis() const { return windDis_; }
    float GetNoiseStrength() const { return noiseStrength_; }
    float GetDisplaceStrength() const { return displaceStrength_; }
    float GetWindNoiseScale() const { return windNoiseScale_; }
    float GetWindNoisePanSpeedX() const { return windNoisePanSpeedX_; }
    float GetWindNoisePanSpeedY() const { return windNoisePanSpeedY_; }
    float GetNoiseFloor() const { return noiseFloor_; }
    float GetWindNoiseScaleStrength() const { return windNoiseScaleStrength_; }

    // ===== 風影パラメータ =====
    void SetWindShadowColor(float r, float g, float b) { windShadowColor_ = { r, g, b }; }
    void SetWindShadowStrength(float v) { windShadowStrength_ = v; }
    void SetWindShadowDispThreshold(float v) { windShadowDispThreshold_ = v; }
    void SetWindShadowSmoothing(float v) { windShadowSmoothing_ = v; }
    void SetFlattenShadowStrength(float v) { flattenShadowStrength_ = v; }
    Float3 GetWindShadowColor() const { return windShadowColor_; }
    float GetWindShadowStrength() const { return windShadowStrength_; }
    float GetWindShadowDispThreshold() const { return windShadowDispThreshold_; }
    float GetWindShadowSmoothing() const { return windShadowSmoothing_; }
    float GetFlattenShadowStrength() const { return flattenShadowStrength_; }

    // ===== インタラクション（踏み倒し） =====
    void SetInteractingObjectPos(float x, float y, float z) { interactingObjPos_ = { x, y, z }; }
    void SetFlattenRadius(float v) { flattenRadius_ = v; }
    void SetFlattenStrength(float v) { flattenStrength_ = v; }
    void SetFlattenFloor(float v) { flattenFloor_ = v; }
    Float3 GetInteractingObjectPos() const { return interactingObjPos_; }
    float GetFlattenRadius() const { return flattenRadius_; }
    float GetFlattenStrength() const { return flattenStrength_; }
    float GetFlattenFloor() const { return flattenFloor_; }

    // ===== ノイズスケール =====
    void SetNoise1Scale(float v) { noise1Scale_ = v; }
    void SetNoise2Scale(float v) { noise2Scale_ = v; }
    void SetCombinedNoiseMinScale(float v) { combinedNoiseMinScale_ = v; }
    void SetCombinedNoiseMaxScale(float v) { combinedNoiseMaxScale_ = v; }
    void SetInvertCombinedNoise(bool v) { invertCombinedNoise_ = v; }
    float GetNoise1Scale() const { return noise1Scale_; }
    float GetNoise2Scale() const { return noise2Scale_; }
    float GetCombinedNoiseMinScale() const { return combinedNoiseMinScale_; }
    float GetCombinedNoiseMaxScale() const { return combinedNoiseMaxScale_; }
    bool GetInvertCombinedNoise() const { return invertCombinedNoise_; }

    // ===== 草サイズ =====
    void SetBaseWidth(float w) { baseWidth_ = w; }
    void SetBaseHeight(float h) { baseHeight_ = h; }
    float GetBaseWidth() const { return baseWidth_; }
    float GetBaseHeight() const { return baseHeight_; }

    // ===== 描画モード =====
    void SetUseGodotShading(bool v) { useGodotShading_ = v; }
    bool GetUseGodotShading() const { return useGodotShading_; }

    // ===== テクスチャ =====
    uint32 GetGrassTextureSRVIndex() const { return grassTextureSRVIndex_; }
    void CreateFallbackTexture(GraphicsDevice* graphics, ID3D12GraphicsCommandList* cmdList);
    bool LoadTextureFromFile(const std::string& filepath);
    const std::string& GetTexturePath() const { return texturePath_; }

    // ===== 後方互換 =====
    void SetWindStrength(float s) { windDis_ = s; }
    float GetWindStrength() const { return windDis_; }

private:
    void CreateNoiseTextures(GraphicsDevice* graphics, ID3D12GraphicsCommandList* cmdList);

    GraphicsDevice* graphics_ = nullptr;
    GrassPipeline pipeline_;
    DynamicConstantBuffer<GrassTransformCB> transformBuffer_;

    float totalTime_ = 0.0f;

    // カラー
    Float3 bottomColor_     = { 0.05f, 0.15f, 0.02f };  // 暗い緑（根元）
    Float3 topColor_        = { 0.2f,  0.5f,  0.1f  };  // 明るい緑（先端）
    Float3 colorVar1_       = { 0.15f, 0.4f,  0.05f };  // バリエーション1
    Float3 colorVar2_       = { 0.1f,  0.35f, 0.08f };  // バリエーション2

    // 風
    float windSpeed_        = 0.005f;
    float windDis_          = 0.2f;
    float noiseStrength_    = 0.5f;
    float displaceStrength_ = 3.0f;

    // 風ノイズ
    float windNoiseScale_   = 20.0f;
    float windNoisePanSpeedX_ = 0.03f;
    float windNoisePanSpeedY_ = 0.03f;
    float noiseFloor_       = 0.3f;
    float windNoiseScaleStrength_ = 0.2f;

    // 風影
    Float3 windShadowColor_ = { 0.0f, 0.2f, 0.0f };
    float windShadowStrength_     = 0.4f;
    float windShadowDispThreshold_ = 0.15f;
    float windShadowSmoothing_     = 0.3f;
    float flattenShadowStrength_   = 0.7f;

    // インタラクション（踏み倒し）
    Float3 interactingObjPos_ = { 0.0f, -1000.0f, 0.0f }; // デフォルトは遠くに
    float flattenRadius_   = 0.75f;
    float flattenStrength_ = 3.0f;
    float flattenFloor_    = 0.3f;

    // ノイズスケール
    float noise1Scale_     = 20.0f;
    float noise2Scale_     = 20.0f;
    float combinedNoiseMinScale_ = 0.5f;
    float combinedNoiseMaxScale_ = 1.5f;
    bool  invertCombinedNoise_ = false;

    // 草サイズ
    float baseWidth_  = 1.0f;
    float baseHeight_ = 1.0f;

    // 描画モード（false=テクスチャベース, true=Godotグラデーション）
    bool useGodotShading_ = false;

    // 草テクスチャ
    Texture2D grassTexture_;
    uint32 grassTextureSRVIndex_ = 0;
    bool hasTexture_ = false;
    bool srvAllocated_ = false;
    std::string texturePath_;

    // ノイズテクスチャ（3枚: 色バリエーション1, 色バリエーション2, 風ノイズ）
    Texture2D noiseTexture1_;
    Texture2D noiseTexture2_;
    Texture2D windNoiseTexture_;
    uint32 noiseTexture1SRVIndex_ = 0;
    uint32 noiseTexture2SRVIndex_ = 0;
    uint32 windNoiseSRVIndex_ = 0;
    bool noiseTexturesCreated_ = false;
};

} // namespace UnoEngine
