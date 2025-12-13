// ParticleUpdateCS.hlsl - Particle Update Compute Shader

#include "ParticleCommon.hlsli"

// バッファ
RWStructuredBuffer<Particle> particlePool : register(u0);
RWStructuredBuffer<uint> deadList : register(u1);
RWStructuredBuffer<uint> aliveListIn : register(u2);
RWStructuredBuffer<uint> aliveListOut : register(u3);
RWByteAddressBuffer counters : register(u4);
RWStructuredBuffer<DrawIndirectArgs> indirectArgs : register(u5);

// 深度バッファ（コリジョン用）
Texture2D<float> depthBuffer : register(t0);
SamplerState depthSampler : register(s0);

// 更新パラメータ
cbuffer UpdateCB : register(b1) {
    float3 gravity;
    float drag;
    float4x4 viewProjMatrix_Collision;
    float4x4 invViewProjMatrix_Collision;
    float2 screenSize;
    uint aliveCountIn;
    uint collisionEnabled;
    float collisionBounce;
    float collisionLifetimeLoss;
    float2 updatePadding;
};

// カウンターオフセット
#define COUNTER_ALIVE_OUT   0
#define COUNTER_DEAD        4

// ワールド座標をスクリーン座標に変換
float3 WorldToScreen(float3 worldPos) {
    float4 clip = mul(float4(worldPos, 1.0f), viewProjMatrix_Collision);
    float3 ndc = clip.xyz / clip.w;
    float2 screen = ndc.xy * 0.5f + 0.5f;
    screen.y = 1.0f - screen.y;  // Y反転
    return float3(screen, ndc.z);
}

// スクリーン座標からワールド座標に変換
float3 ScreenToWorld(float2 screen, float depth) {
    screen.y = 1.0f - screen.y;
    float2 ndc = screen * 2.0f - 1.0f;
    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, invViewProjMatrix_Collision);
    return world.xyz / world.w;
}

// 深度バッファコリジョン
bool CheckDepthCollision(float3 position, float3 velocity, out float3 normal, out float penetration) {
    float3 screenPos = WorldToScreen(position);

    // 画面外チェック
    if (screenPos.x < 0 || screenPos.x > 1 || screenPos.y < 0 || screenPos.y > 1) {
        normal = float3(0, 0, 0);
        penetration = 0;
        return false;
    }

    float sceneDepth = depthBuffer.SampleLevel(depthSampler, screenPos.xy, 0);

    // 深度比較（Z-Near: 0, Z-Far: 1 の場合）
    float depthDiff = screenPos.z - sceneDepth;

    if (depthDiff > 0.0001f) {
        // コリジョン発生
        penetration = depthDiff;

        // 法線推定（周囲のピクセルから勾配を計算）
        float2 texelSize = 1.0f / screenSize;
        float depthL = depthBuffer.SampleLevel(depthSampler, screenPos.xy + float2(-texelSize.x, 0), 0);
        float depthR = depthBuffer.SampleLevel(depthSampler, screenPos.xy + float2(texelSize.x, 0), 0);
        float depthD = depthBuffer.SampleLevel(depthSampler, screenPos.xy + float2(0, -texelSize.y), 0);
        float depthU = depthBuffer.SampleLevel(depthSampler, screenPos.xy + float2(0, texelSize.y), 0);

        float3 posL = ScreenToWorld(screenPos.xy + float2(-texelSize.x, 0), depthL);
        float3 posR = ScreenToWorld(screenPos.xy + float2(texelSize.x, 0), depthR);
        float3 posD = ScreenToWorld(screenPos.xy + float2(0, -texelSize.y), depthD);
        float3 posU = ScreenToWorld(screenPos.xy + float2(0, texelSize.y), depthU);

        float3 tangentX = posR - posL;
        float3 tangentY = posU - posD;
        normal = normalize(cross(tangentX, tangentY));

        // 法線が速度と同じ方向を向いていたら反転
        if (dot(normal, velocity) > 0) {
            normal = -normal;
        }

        return true;
    }

    normal = float3(0, 0, 0);
    penetration = 0;
    return false;
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    if (DTid.x >= aliveCountIn) {
        return;
    }

    // Alive Listからパーティクルインデックスを取得
    uint particleIndex = aliveListIn[DTid.x];
    Particle p = particlePool[particleIndex];

    // 非アクティブなパーティクルはスキップ
    if ((p.flags & PARTICLE_FLAG_ACTIVE) == 0) {
        return;
    }

    // 寿命更新
    p.lifetime += deltaTime;

    // 寿命チェック
    if (p.lifetime >= p.maxLifetime) {
        // パーティクル死亡
        p.flags &= ~PARTICLE_FLAG_ACTIVE;
        particlePool[particleIndex] = p;

        // Dead Listに追加
        uint deadIndex;
        counters.InterlockedAdd(COUNTER_DEAD, 1, deadIndex);
        deadList[deadIndex] = particleIndex;
        return;
    }

    // 力の適用（重力）
    p.velocity += gravity * deltaTime;

    // ドラッグ（空気抵抗）
    p.velocity *= (1.0f - drag * deltaTime);

    // 位置更新
    float3 newPosition = p.position + p.velocity * deltaTime;

    // コリジョン
    if (collisionEnabled != 0 && (p.flags & PARTICLE_FLAG_COLLISION_ENABLED) != 0) {
        float3 collisionNormal;
        float penetration;

        if (CheckDepthCollision(newPosition, p.velocity, collisionNormal, penetration)) {
            // 反射
            float3 reflectedVel = reflect(p.velocity, collisionNormal);
            p.velocity = reflectedVel * collisionBounce;

            // 位置補正（めり込み解消）
            newPosition = p.position + collisionNormal * penetration * 0.1f;

            // 寿命減少
            p.lifetime += p.maxLifetime * collisionLifetimeLoss;
        }
    }

    p.position = newPosition;

    // 回転更新
    p.rotation += p.angularVelocity * deltaTime;

    // パーティクル更新
    particlePool[particleIndex] = p;

    // Alive List (Out) に追加
    uint aliveIndex;
    counters.InterlockedAdd(COUNTER_ALIVE_OUT, 1, aliveIndex);
    aliveListOut[aliveIndex] = particleIndex;
}

// Indirect引数を構築するシェーダー
[numthreads(1, 1, 1)]
void BuildIndirectArgs(uint3 DTid : SV_DispatchThreadID) {
    uint aliveCount;
    counters.InterlockedAdd(COUNTER_ALIVE_OUT, 0, aliveCount);

    DrawIndirectArgs args;
    args.indexCountPerInstance = 6;  // クアッド（2三角形）
    args.instanceCount = aliveCount;
    args.startIndexLocation = 0;
    args.baseVertexLocation = 0;
    args.startInstanceLocation = 0;

    indirectArgs[0] = args;
}
