// Vignette Post Process Pixel Shader

cbuffer VignetteParams : register(b0) {
    float radius;
    float softness;
    float intensity;
    float padding;
};

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float4 color = inputTexture.Sample(linearSampler, input.texCoord);
    
    // UV座標を中心基準に変換 (-0.5 ~ 0.5)
    float2 position = input.texCoord - 0.5f;
    
    // 中心からの距離を計算
    float dist = length(position);
    
    // smoothstepでビネット値を計算
    // radius: 減衰開始位置, radius - softness: 完全に暗くなる位置
    float vignette = smoothstep(radius, radius - softness, dist);
    
    // intensityで強度を調整
    vignette = lerp(1.0f, vignette, intensity);
    
    return float4(color.rgb * vignette, color.a);
}
