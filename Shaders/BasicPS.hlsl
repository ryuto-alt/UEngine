// Basic Pixel Shader

struct PSInput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    float3 lightDir = normalize(float3(0.5f, -1.0f, 0.5f));
    float diffuse = max(dot(input.normal, -lightDir), 0.2f);
    float3 color = float3(0.8f, 0.8f, 0.8f) * diffuse;
    return float4(color, 1.0f);
}
