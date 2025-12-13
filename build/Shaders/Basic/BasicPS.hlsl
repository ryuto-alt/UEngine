// Basic Pixel Shader

Texture2D albedoTexture : register(t0);
SamplerState albedoSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    float3 albedo = albedoTexture.Sample(albedoSampler, input.uv).rgb;
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float diffuse = max(dot(input.normal, -lightDir), 0.2f);
    float3 color = albedo * diffuse;
    return float4(color, 1.0f);
}
