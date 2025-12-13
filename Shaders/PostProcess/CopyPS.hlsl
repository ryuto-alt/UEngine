// Copy/Passthrough Post Process Pixel Shader

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    return inputTexture.Sample(linearSampler, input.texCoord);
}
