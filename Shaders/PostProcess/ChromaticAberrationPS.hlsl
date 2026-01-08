// Chromatic Aberration Post Process Pixel Shader
// RGB各チャネルを異なるオフセットでサンプリングし、レンズの色収差を再現

cbuffer ChromaticAberrationParams : register(b0) {
    float intensity;     // 全体の強度 (0-1)
    float redOffset;     // 赤チャネルのオフセット
    float greenOffset;   // 緑チャネルのオフセット（通常0）
    float blueOffset;    // 青チャネルのオフセット
};

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    // 画面中心からの方向ベクトル
    float2 center = float2(0.5f, 0.5f);
    float2 direction = input.texCoord - center;

    // 各チャネルを異なるオフセットでサンプリング
    float r = inputTexture.Sample(linearSampler, input.texCoord + direction * redOffset * intensity).r;
    float g = inputTexture.Sample(linearSampler, input.texCoord + direction * greenOffset * intensity).g;
    float b = inputTexture.Sample(linearSampler, input.texCoord + direction * blueOffset * intensity).b;

    // アルファは元のテクスチャから
    float a = inputTexture.Sample(linearSampler, input.texCoord).a;

    return float4(r, g, b, a);
}
