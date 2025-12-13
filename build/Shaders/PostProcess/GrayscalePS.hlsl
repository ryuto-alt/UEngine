// Grayscale Post Process Pixel Shader

Texture2D inputTexture : register(t0);
SamplerState linearSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float4 color = inputTexture.Sample(linearSampler, input.texCoord);
    
    // ITU-R BT.709 luminance coefficients (perceptually accurate)
    float luminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    
    return float4(luminance, luminance, luminance, color.a);
}
