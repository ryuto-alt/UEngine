// ParticleCommon.hlsli - GPU Particle System Common Definitions

#ifndef PARTICLE_COMMON_HLSLI
#define PARTICLE_COMMON_HLSLI

// ================================
// パーティクルデータ構造体
// ================================
struct Particle {
    float3 position;
    float lifetime;             // 現在の経過時間
    float3 velocity;
    float maxLifetime;          // 最大寿命
    float4 color;               // RGBA
    float2 size;                // width, height
    float rotation;             // Z軸回転（ラジアン）
    float angularVelocity;      // 角速度
    uint emitterID;             // 所属エミッターID
    uint flags;                 // ビットフラグ
    float2 uvOffset;            // スプライトシートオフセット
    float2 uvScale;             // スプライトシートスケール
    float random;               // パーティクル固有の乱数（0-1）
    float3 padding;             // アライメント用
};

// パーティクルフラグ
#define PARTICLE_FLAG_ACTIVE            (1 << 0)
#define PARTICLE_FLAG_BILLBOARD         (1 << 1)
#define PARTICLE_FLAG_MESH              (1 << 2)
#define PARTICLE_FLAG_TRAIL             (1 << 3)
#define PARTICLE_FLAG_COLLISION_ENABLED (1 << 4)

// ================================
// カウンター構造体
// ================================
struct ParticleCounters {
    uint aliveCount;
    uint deadCount;
    uint emitCount;
    uint padding;
};

// ================================
// Indirect描画引数
// ================================
struct DrawIndirectArgs {
    uint indexCountPerInstance;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

// ================================
// エミッター設定（GPU用簡易版）
// ================================
struct EmitterParams {
    float3 position;
    float emitRate;
    float3 minVelocity;
    float deltaTime;
    float3 maxVelocity;
    float time;                 // エミッター経過時間
    float minLifetime;
    float maxLifetime;
    float minSize;
    float maxSize;
    float4 startColor;
    float3 gravity;
    float drag;
    uint emitterID;
    uint maxParticles;
    uint emitShape;             // EmitShape enum
    uint flags;
    float shapeRadius;
    float coneAngle;
    float2 shapePadding;
};

// エミッション形状
#define EMIT_SHAPE_POINT        0
#define EMIT_SHAPE_SPHERE       1
#define EMIT_SHAPE_HEMISPHERE   2
#define EMIT_SHAPE_BOX          3
#define EMIT_SHAPE_CONE         4
#define EMIT_SHAPE_CIRCLE       5

// ================================
// ユーティリティ関数
// ================================

// パーティクルが生存しているか
bool IsParticleAlive(Particle p) {
    return (p.flags & PARTICLE_FLAG_ACTIVE) != 0 && p.lifetime < p.maxLifetime;
}

// ライフタイム進行率（0-1）
float GetLifetimeProgress(Particle p) {
    return saturate(p.lifetime / max(p.maxLifetime, 0.0001f));
}

// 疑似乱数生成（Wang Hash）
uint WangHash(uint seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

// float乱数（0-1）
float RandomFloat(uint seed) {
    return float(WangHash(seed)) / 4294967295.0f;
}

// float乱数（範囲指定）
float RandomRange(uint seed, float minVal, float maxVal) {
    return lerp(minVal, maxVal, RandomFloat(seed));
}

// 3D乱数ベクトル（-1 to 1）
float3 RandomDirection(uint seed) {
    float theta = RandomFloat(seed) * 6.28318530718f;
    float phi = acos(2.0f * RandomFloat(seed + 1) - 1.0f);
    float sinPhi = sin(phi);
    return float3(
        sinPhi * cos(theta),
        sinPhi * sin(theta),
        cos(phi)
    );
}

// 球内のランダム位置
float3 RandomInSphere(uint seed, float radius) {
    float3 dir = RandomDirection(seed);
    float r = pow(RandomFloat(seed + 3), 1.0f / 3.0f) * radius;
    return dir * r;
}

// 球表面のランダム位置
float3 RandomOnSphere(uint seed, float radius) {
    return RandomDirection(seed) * radius;
}

// 円錐内のランダム方向
float3 RandomInCone(uint seed, float3 direction, float angleRadians) {
    float cosAngle = cos(angleRadians);
    float z = RandomRange(seed, cosAngle, 1.0f);
    float phi = RandomFloat(seed + 1) * 6.28318530718f;
    float sinZ = sqrt(1.0f - z * z);

    float3 randomDir = float3(sinZ * cos(phi), sinZ * sin(phi), z);

    // direction方向に回転
    float3 up = abs(direction.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 right = normalize(cross(up, direction));
    up = cross(direction, right);

    return randomDir.x * right + randomDir.y * up + randomDir.z * direction;
}

// Box内のランダム位置
float3 RandomInBox(uint seed, float3 halfExtents) {
    return float3(
        RandomRange(seed, -halfExtents.x, halfExtents.x),
        RandomRange(seed + 1, -halfExtents.y, halfExtents.y),
        RandomRange(seed + 2, -halfExtents.z, halfExtents.z)
    );
}

// カラー補間
float4 LerpColor(float4 a, float4 b, float t) {
    return a + (b - a) * t;
}

// ================================
// 定数バッファ定義
// ================================
cbuffer ParticleSystemCB : register(b0) {
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float4x4 viewProjMatrix;
    float4x4 invViewMatrix;
    float3 cameraPosition;
    float totalTime;
    float3 cameraRight;
    float deltaTime;
    float3 cameraUp;
    uint frameIndex;
};

#endif // PARTICLE_COMMON_HLSLI
