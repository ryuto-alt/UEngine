// ParticleBillboardVS.hlsl - Billboard Particle Vertex Shader

#include "ParticleCommon.hlsli"

// パーティクルバッファ
StructuredBuffer<Particle> particlePool : register(t0);
StructuredBuffer<uint> aliveList : register(t1);

// 出力構造体
struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float lifetime : TEXCOORD1;
};

VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID) {
    VSOutput output;

    // Alive Listからパーティクルインデックスを取得
    uint particleIndex = aliveList[instanceID];
    Particle p = particlePool[particleIndex];

    // クアッドの頂点（0-3）
    // 0: 左下, 1: 右下, 2: 左上, 3: 右上
    uint quadVertex = vertexID % 4;

    // UV座標
    float2 uv;
    uv.x = (quadVertex == 1 || quadVertex == 3) ? 1.0f : 0.0f;
    uv.y = (quadVertex == 2 || quadVertex == 3) ? 0.0f : 1.0f;

    // スプライトシートUV適用
    output.texCoord = p.uvOffset + uv * p.uvScale;

    // ローカルオフセット（-0.5 to 0.5）
    float2 localOffset;
    localOffset.x = (quadVertex == 1 || quadVertex == 3) ? 0.5f : -0.5f;
    localOffset.y = (quadVertex == 2 || quadVertex == 3) ? 0.5f : -0.5f;

    // サイズ適用
    localOffset *= p.size;

    // 回転適用
    float cosR = cos(p.rotation);
    float sinR = sin(p.rotation);
    float2 rotatedOffset;
    rotatedOffset.x = localOffset.x * cosR - localOffset.y * sinR;
    rotatedOffset.y = localOffset.x * sinR + localOffset.y * cosR;

    // ビルボード: カメラのright/upベクトルを使用
    float3 worldOffset = cameraRight * rotatedOffset.x + cameraUp * rotatedOffset.y;
    float3 worldPos = p.position + worldOffset;

    // MVP変換
    output.position = mul(float4(worldPos, 1.0f), viewProjMatrix);

    // ライフタイム進行に応じた色（将来的にカーブで制御）
    float lifetimeProgress = GetLifetimeProgress(p);
    output.color = p.color;
    output.lifetime = lifetimeProgress;

    return output;
}
