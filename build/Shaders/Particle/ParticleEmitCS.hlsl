// ParticleEmitCS.hlsl - Particle Emission Compute Shader

#include "ParticleCommon.hlsli"

// バッファ
RWStructuredBuffer<Particle> particlePool : register(u0);
RWStructuredBuffer<uint> deadList : register(u1);
RWStructuredBuffer<uint> aliveList : register(u2);
RWByteAddressBuffer counters : register(u3);

// エミッターパラメータ
cbuffer EmitterCB : register(b1) {
    EmitterParams emitter;
};

// カウンターオフセット
#define COUNTER_ALIVE   0
#define COUNTER_DEAD    4
#define COUNTER_EMIT    8

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    // 発生させるパーティクル数を取得
    uint emitCount;
    counters.InterlockedAdd(COUNTER_EMIT, 0, emitCount);

    if (DTid.x >= emitCount) {
        return;
    }

    // Dead Listからスロットを取得
    uint deadCount;
    counters.InterlockedAdd(COUNTER_DEAD, -1, deadCount);

    if (deadCount == 0) {
        // スロットが無い場合は復元
        counters.InterlockedAdd(COUNTER_DEAD, 1, deadCount);
        return;
    }

    uint slotIndex = deadList[deadCount - 1];

    // 乱数シード（スレッドID、フレーム、時間を組み合わせ）
    uint seed = WangHash(DTid.x + frameIndex * 1000 + uint(totalTime * 1000.0f));

    // 新しいパーティクルを初期化
    Particle p;
    p.emitterID = emitter.emitterID;
    p.flags = PARTICLE_FLAG_ACTIVE | PARTICLE_FLAG_BILLBOARD;
    p.random = RandomFloat(seed);

    // 寿命
    p.lifetime = 0.0f;
    p.maxLifetime = RandomRange(seed + 10, emitter.minLifetime, emitter.maxLifetime);

    // 位置と速度（エミッション形状による）
    float3 localPos = float3(0, 0, 0);
    float3 direction = float3(0, 1, 0);

    switch (emitter.emitShape) {
    case EMIT_SHAPE_POINT:
        localPos = float3(0, 0, 0);
        direction = RandomDirection(seed + 20);
        break;

    case EMIT_SHAPE_SPHERE:
        localPos = RandomInSphere(seed + 20, emitter.shapeRadius);
        direction = normalize(localPos);
        break;

    case EMIT_SHAPE_HEMISPHERE:
        localPos = RandomInSphere(seed + 20, emitter.shapeRadius);
        if (localPos.y < 0) localPos.y = -localPos.y;
        direction = normalize(localPos);
        break;

    case EMIT_SHAPE_CONE:
        direction = RandomInCone(seed + 20, float3(0, 1, 0), radians(emitter.coneAngle));
        localPos = float3(0, 0, 0);
        break;

    case EMIT_SHAPE_CIRCLE:
        {
            float angle = RandomFloat(seed + 20) * 6.28318530718f;
            float r = sqrt(RandomFloat(seed + 21)) * emitter.shapeRadius;
            localPos = float3(cos(angle) * r, 0, sin(angle) * r);
            direction = float3(0, 1, 0);
        }
        break;

    default:
        localPos = float3(0, 0, 0);
        direction = RandomDirection(seed + 20);
        break;
    }

    p.position = emitter.position + localPos;

    // 速度
    float speed = RandomRange(seed + 30, length(emitter.minVelocity), length(emitter.maxVelocity));
    p.velocity = direction * speed;

    // サイズ
    float size = RandomRange(seed + 40, emitter.minSize, emitter.maxSize);
    p.size = float2(size, size);

    // 色
    p.color = emitter.startColor;

    // 回転
    p.rotation = 0.0f;
    p.angularVelocity = 0.0f;

    // UV（デフォルト）
    p.uvOffset = float2(0, 0);
    p.uvScale = float2(1, 1);

    // パディング
    p.padding = float3(0, 0, 0);

    // パーティクルプールに書き込み
    particlePool[slotIndex] = p;

    // Alive Listに追加
    uint aliveIndex;
    counters.InterlockedAdd(COUNTER_ALIVE, 1, aliveIndex);
    aliveList[aliveIndex] = slotIndex;
}
