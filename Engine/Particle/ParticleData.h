#pragma once

#include "../Core/Types.h"
#include "../Math/MathCommon.h"
#include "Curve.h"
#include "Gradient.h"
#include <vector>
#include <string>

namespace UnoEngine {

// パーティクルデータ（GPU側と一致させる）
struct alignas(16) GPUParticle {
    Float3 position;
    float lifetime;             // 現在の経過時間
    Float3 velocity;
    float maxLifetime;          // 最大寿命
    Float4 color;               // RGBA
    Float2 size;                // width, height
    float rotation;             // Z軸回転（ラジアン）
    float angularVelocity;      // 角速度
    uint32 emitterID;           // 所属エミッターID
    uint32 flags;               // ビットフラグ
    Float2 uvOffset;            // スプライトシートオフセット
    Float2 uvScale;             // スプライトシートスケール
    float random;               // パーティクル固有の乱数（0-1）
    float padding[3];           // アライメント用パディング
};

// パーティクルフラグ
namespace ParticleFlags {
    constexpr uint32 Active = 1 << 0;
    constexpr uint32 Billboard = 1 << 1;
    constexpr uint32 Mesh = 1 << 2;
    constexpr uint32 Trail = 1 << 3;
    constexpr uint32 CollisionEnabled = 1 << 4;
}

// エミッション形状
enum class EmitShape {
    Point,      // 点
    Sphere,     // 球
    Hemisphere, // 半球
    Box,        // ボックス
    Cone,       // 円錐
    Circle,     // 円
    Edge        // 線
};

// エミッション形状パラメータ
struct ShapeConfig {
    EmitShape shape = EmitShape::Point;
    float radius = 1.0f;            // Sphere, Hemisphere, Cone, Circle
    Float3 boxSize = { 1.0f, 1.0f, 1.0f };  // Box
    float coneAngle = 25.0f;        // Cone（度）
    float coneRadius = 1.0f;        // Cone底面半径
    float arcAngle = 360.0f;        // Circle/Cone の円弧角度
    Float3 position = { 0.0f, 0.0f, 0.0f };  // ローカルオフセット
    Float3 rotation = { 0.0f, 0.0f, 0.0f };  // ローカル回転
    bool emitFromEdge = false;      // 表面から放出
    bool randomDirection = false;   // ランダム方向
};

// バースト設定
struct BurstConfig {
    float time = 0.0f;          // 発生時刻（秒）
    int32 count = 10;           // 発生数
    int32 cycles = 1;           // 繰り返し回数（0=無限）
    float interval = 0.0f;      // 繰り返し間隔
    float probability = 1.0f;   // 発生確率（0-1）
};

// レンダリングモード
enum class RenderMode {
    Billboard,      // ビルボード（カメラに正対）
    StretchedBillboard,  // 速度方向に伸びるビルボード
    HorizontalBillboard, // 水平ビルボード
    VerticalBillboard,   // 垂直ビルボード
    Mesh,           // 3Dメッシュ
    Trail           // トレイル
};

// ブレンドモード
enum class BlendMode {
    Additive,       // 加算合成
    AlphaBlend,     // アルファブレンド
    Multiply,       // 乗算
    Premultiplied   // プリマルチプライドアルファ
};

// プロシージャルシェイプタイプ（テクスチャなしでシェーダーで描画）
enum class ProceduralShape {
    None,           // 通常（テクスチャまたは円形グラデーション）
    Circle,         // 単純な円
    Ring,           // リング（ドーナツ）
    Star,           // 星形
    Pentagon,       // 五角形
    Hexagon,        // 六角形
    MagicCircle,    // 魔法陣（複合）
    Rune,           // ルーン文字風
    Cross,          // 十字
    Sparkle         // きらめき
};

// スプライトシート設定
struct SpriteSheetConfig {
    bool enabled = false;
    int32 tilesX = 1;           // 横タイル数
    int32 tilesY = 1;           // 縦タイル数
    int32 frameCount = 1;       // 総フレーム数
    AnimationCurve frameOverTime;  // 時間によるフレーム変化
    float fps = 30.0f;          // フレームレート（frameOverTimeが無い場合）
    int32 startFrame = 0;       // 開始フレーム
    bool loop = true;           // ループ
};

// コリジョン設定
struct CollisionConfig {
    bool enabled = false;
    float bounce = 0.5f;        // 反発係数
    float lifetimeLoss = 0.0f;  // 衝突時の寿命減少率
    float minKillSpeed = 0.0f;  // この速度以下で死亡
    bool killOnCollision = false;  // 衝突時即死
    float radiusScale = 1.0f;   // コリジョン半径スケール
};

// サブエミッター設定
struct SubEmitterConfig {
    enum class Trigger {
        Birth,      // 生成時
        Death,      // 死亡時
        Collision   // 衝突時
    };

    Trigger trigger = Trigger::Death;
    std::string emitterName;    // 参照するエミッター名
    int32 emitCount = 1;        // 発生数
    float probability = 1.0f;   // 発生確率
};

// 時間経過による速度変化
struct VelocityOverLifetime {
    bool enabled = false;
    MinMaxCurve speedMultiplier = MinMaxCurve::Constant(1.0f);
    MinMaxCurve x = MinMaxCurve::Constant(0.0f);
    MinMaxCurve y = MinMaxCurve::Constant(0.0f);
    MinMaxCurve z = MinMaxCurve::Constant(0.0f);
    bool isLocal = false;  // ローカル座標系で適用
};

// 時間経過による色変化
struct ColorOverLifetime {
    bool enabled = false;
    MinMaxGradient color = MinMaxGradient::Color({ 1.0f, 1.0f, 1.0f, 1.0f });
};

// 時間経過によるサイズ変化
struct SizeOverLifetime {
    bool enabled = false;
    MinMaxCurve size = MinMaxCurve::Constant(1.0f);
    bool separateAxes = false;
    MinMaxCurve x = MinMaxCurve::Constant(1.0f);
    MinMaxCurve y = MinMaxCurve::Constant(1.0f);
};

// 時間経過による回転変化
struct RotationOverLifetime {
    bool enabled = false;
    MinMaxCurve angularVelocity = MinMaxCurve::Constant(0.0f);  // 度/秒
};

// 力（重力、風など）
struct ForceOverLifetime {
    bool enabled = false;
    MinMaxCurve x = MinMaxCurve::Constant(0.0f);
    MinMaxCurve y = MinMaxCurve::Constant(-9.8f);  // デフォルトで重力
    MinMaxCurve z = MinMaxCurve::Constant(0.0f);
    bool isLocal = false;
};

// ノイズモジュール
struct NoiseModule {
    bool enabled = false;
    float strength = 1.0f;
    float frequency = 0.5f;
    int32 octaves = 1;
    float scrollSpeed = 0.0f;
    bool separateAxes = false;
    float strengthX = 1.0f;
    float strengthY = 1.0f;
    float strengthZ = 1.0f;
};


// フォースフィールドタイプ
enum class ForceFieldType {
    Directional,    // 一方向の力
    Radial,         // 放射状の力（中心から外/内）
    Vortex,         // 渦巻き
    Turbulence,     // 乱流
    Drag            // 抵抗
};

// フォースフィールド形状
enum class ForceFieldShape {
    Infinite,       // 無限（全体に適用）
    Sphere,         // 球
    Box,            // ボックス
    Cylinder        // 円柱
};

// 単一のフォースフィールド
struct ForceField {
    bool enabled = false;
    ForceFieldType type = ForceFieldType::Directional;
    ForceFieldShape shape = ForceFieldShape::Infinite;
    
    // 位置・サイズ
    Float3 position = { 0.0f, 0.0f, 0.0f };
    Float3 size = { 5.0f, 5.0f, 5.0f };     // 球=radius使用、ボックス=各軸
    float radius = 5.0f;
    
    // 力のパラメータ
    Float3 direction = { 0.0f, 1.0f, 0.0f }; // Directional用
    float strength = 1.0f;                   // 力の強さ
    float attenuation = 0.0f;                // 減衰率（距離による）
    
    // Vortex（渦巻き）用パラメータ
    Float3 axis = { 0.0f, 1.0f, 0.0f };      // 渦の軸
    float rotationSpeed = 1.0f;              // 回転速度
    float inwardForce = 0.0f;                // 内向きの力
    float upwardForce = 0.0f;                // 上向きの力
    
    // Turbulence用
    float frequency = 1.0f;
    int32 octaves = 1;
    
    // Drag用
    float dragCoefficient = 0.1f;
};

// フォースフィールドモジュール（複数のフォースフィールドを持てる）
struct ForceFieldModule {
    bool enabled = false;
    std::vector<ForceField> fields;
};

// アトラクター（引力/斥力）
struct Attractor {
    bool enabled = false;
    Float3 position = { 0.0f, 0.0f, 0.0f };
    float strength = 5.0f;                   // 正=引力、負=斥力
    float radius = 10.0f;                    // 影響範囲
    float deadzone = 0.5f;                   // この距離以下では力が働かない
    bool killOnContact = false;              // 接触時にパーティクルを消す
    float innerRadius = 0.0f;                // 内側の範囲（この内側では最大力）
};

// アトラクターモジュール
struct AttractorModule {
    bool enabled = false;
    std::vector<Attractor> attractors;
};

// 軌道運動モジュール（Effekseerのリング/スパイラル移動）
struct OrbitalModule {
    bool enabled = false;
    Float3 center = { 0.0f, 0.0f, 0.0f };    // 軌道の中心
    Float3 axis = { 0.0f, 1.0f, 0.0f };      // 回転軸
    MinMaxCurve angularVelocity = MinMaxCurve::Constant(180.0f);  // 度/秒
    MinMaxCurve radialVelocity = MinMaxCurve::Constant(0.0f);     // 半径方向の速度
    float startRadius = 1.0f;                // 初期半径
    bool inheritEmitterRotation = false;
};

// リボン/トレイルのレンダリング設定（拡張版）
struct RibbonConfig {
    bool enabled = false;
    int32 segments = 20;                     // セグメント数
    float length = 2.0f;                     // リボン長さ
    float width = 0.5f;                      // リボン幅
    MinMaxCurve widthOverLength = MinMaxCurve::Constant(1.0f);
    MinMaxGradient colorOverLength;
    bool faceCameraAxis = true;              // カメラ軸に向ける
    float uvRepeat = 1.0f;                   // UVの繰り返し回数
};

// 親子関係設定（Effekseerのノードシステム）
struct ParentChildConfig {
    bool enabled = false;
    bool inheritPosition = true;
    bool inheritRotation = true;
    bool inheritScale = true;
    bool inheritColor = false;
    float followDelay = 0.0f;               // 追従遅延
};

// トレイル設定
struct TrailConfig {
    bool enabled = false;
    float lifetime = 0.5f;          // トレイル寿命
    float minVertexDistance = 0.1f; // 頂点追加の最小距離
    int32 maxPoints = 50;           // 最大点数
    float widthMultiplier = 1.0f;   // 幅倍率
    MinMaxCurve widthOverTrail = MinMaxCurve::Constant(1.0f);
    MinMaxGradient colorOverTrail;
    bool inheritParticleColor = true;
    bool dieWithParticle = true;
};

// エミッター設定（全パラメータ）
struct EmitterConfig {
    std::string name = "Emitter";

    // 基本設定
    float duration = 5.0f;          // エミッター持続時間
    bool looping = true;            // ループ
    bool prewarm = false;           // プレウォーム（開始時にパーティクル充填）
    float startDelay = 0.0f;        // 開始遅延
    uint32 maxParticles = 1000;     // このエミッターの最大パーティクル数

    // 生成設定
    float emitRate = 10.0f;         // パーティクル/秒
    std::vector<BurstConfig> bursts;
    ShapeConfig shape;

    // 初期値
    MinMaxCurve startLifetime = MinMaxCurve::Range(3.0f, 5.0f);
    MinMaxCurve startSpeed = MinMaxCurve::Range(1.0f, 2.0f);
    MinMaxCurve startSize = MinMaxCurve::Range(0.5f, 1.0f);
    MinMaxGradient startColor = MinMaxGradient::Color({ 1.0f, 1.0f, 1.0f, 1.0f });
    MinMaxCurve startRotation = MinMaxCurve::Constant(0.0f);  // 度

    // 時間経過モジュール
    VelocityOverLifetime velocityOverLifetime;
    ColorOverLifetime colorOverLifetime;
    SizeOverLifetime sizeOverLifetime;
    RotationOverLifetime rotationOverLifetime;
    ForceOverLifetime forceOverLifetime;
    NoiseModule noise;
    
    // 3Dエフェクト用モジュール
    ForceFieldModule forceField;
    AttractorModule attractor;
    OrbitalModule orbital;
    RibbonConfig ribbon;
    ParentChildConfig parentChild;

    // レンダリング
    RenderMode renderMode = RenderMode::Billboard;
    BlendMode blendMode = BlendMode::Additive;
    ProceduralShape proceduralShape = ProceduralShape::None;  // プロシージャル形状
    float proceduralParam1 = 0.5f;   // 形状パラメータ1（リングの太さ、星の内側半径など）
    float proceduralParam2 = 5.0f;   // 形状パラメータ2（星の頂点数など）
    std::string texturePath;
    SpriteSheetConfig spriteSheet;
    float sortingFudge = 0.0f;      // ソート調整値

    // コリジョン
    CollisionConfig collision;

    // サブエミッター
    std::vector<SubEmitterConfig> subEmitters;

    // トレイル
    TrailConfig trail;
};

// カウンター構造体（GPU側で使用）
struct alignas(16) ParticleCounters {
    uint32 aliveCount;
    uint32 deadCount;
    uint32 emitCount;
    uint32 padding;
};

// Indirect引数（DrawIndexedInstancedIndirect用）
struct alignas(16) DrawIndirectArgs {
    uint32 indexCountPerInstance;
    uint32 instanceCount;
    uint32 startIndexLocation;
    int32 baseVertexLocation;
    uint32 startInstanceLocation;
};

} // namespace UnoEngine
