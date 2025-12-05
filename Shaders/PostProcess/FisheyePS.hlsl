// Fisheye (Barrel Distortion) Post Process Pixel Shader

cbuffer FisheyeParams : register(b0) {
    float strength;  // 歪みの強さ
    float zoom;      // ズーム倍率
    float2 padding;
};

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    // UV座標を中心基準に変換 (-1 ~ 1)
    float2 uv = input.texCoord * 2.0f - 1.0f;
    
    // 中心からの距離
    float r = length(uv);
    
    // Barrel distortion formula
    // 距離の2乗に比例して歪みを適用
    float distortion = 1.0f + r * r * strength;
    
    // 歪んだUV座標を計算
    float2 distortedUV = uv * distortion / zoom;
    
    // UV座標を0-1に戻す
    distortedUV = distortedUV * 0.5f + 0.5f;
    
    // 範囲外は黒を返す
    if (distortedUV.x < 0.0f || distortedUV.x > 1.0f ||
        distortedUV.y < 0.0f || distortedUV.y > 1.0f) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    return inputTexture.Sample(linearSampler, distortedUV);
}
