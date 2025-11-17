// Sprite Pixel Shader

Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

cbuffer SpriteColor : register(b0) {
    float4 tintColor;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    float4 texColor = spriteTexture.Sample(spriteSampler, input.uv);
    return texColor * tintColor;
}
