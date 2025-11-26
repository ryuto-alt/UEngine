// Skinned Pixel Shader - Lambert Lighting

Texture2D diffuseTexture : register(t0);
SamplerState samplerState : register(s0);

cbuffer Light : register(b1) {
    float3 lightDirection;
    float padding0;
    float3 lightColor;
    float lightIntensity;
    float3 ambientLight;
    float padding1;
    float3 cameraPosition;
    float padding2;
};

cbuffer Material : register(b2) {
    float3 albedo;
    float metallic;
    float roughness;
    float3 padding3;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET {
    // Sample texture
    float4 texColor = diffuseTexture.Sample(samplerState, input.uv);

    // Normalize vectors
    float3 N = normalize(input.normal);
    float3 L = normalize(-lightDirection);

    // Lambert diffuse
    float NdotL = max(dot(N, L), 0.0f);
    float3 diffuse = lightColor * lightIntensity * NdotL;

    // Combine lighting
    float3 finalColor = texColor.rgb * albedo * (ambientLight + diffuse);

    return float4(finalColor, texColor.a);
}
