// ParticlePS.hlsl - Particle Pixel Shader

// テクスチャ
Texture2D particleTexture : register(t2);
SamplerState particleSampler : register(s0);

// 定数バッファ
cbuffer ParticleRenderCB : register(b2) {
    uint useTexture;        // テクスチャ使用フラグ
    uint blendMode;         // ブレンドモード
    float softParticleScale; // ソフトパーティクルスケール
    float padding;
};

// ブレンドモード定数
#define BLEND_ADDITIVE      0
#define BLEND_ALPHA         1
#define BLEND_MULTIPLY      2
#define BLEND_PREMULTIPLIED 3

// 入力構造体
struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float lifetime : TEXCOORD1;
};

float4 main(PSInput input) : SV_TARGET {
    float4 texColor = float4(1, 1, 1, 1);

    if (useTexture != 0) {
        texColor = particleTexture.Sample(particleSampler, input.texCoord);
    }

    // 最終カラー
    float4 finalColor = texColor * input.color;

    // ブレンドモードによる調整
    switch (blendMode) {
    case BLEND_ADDITIVE:
        // 加算合成: アルファは輝度として扱う
        finalColor.rgb *= finalColor.a;
        finalColor.a = 1.0f;  // アルファブレンディングは別途設定
        break;

    case BLEND_ALPHA:
        // 通常のアルファブレンド
        break;

    case BLEND_MULTIPLY:
        // 乗算: 出力は後でブレンドステートで処理
        break;

    case BLEND_PREMULTIPLIED:
        // プリマルチプライドアルファ（既にRGBにアルファが掛かっている）
        break;
    }

    // アルファが0に近い場合は破棄
    if (finalColor.a < 0.001f) {
        discard;
    }

    return finalColor;
}
