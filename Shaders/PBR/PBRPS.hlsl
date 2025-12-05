// PBR Pixel Shader

Texture2D albedoTexture : register(t0);
SamplerState albedoSampler : register(s0);

cbuffer LightData : register(b1) {
    float3 directionalLightDirection;
    float padding0;
    float3 directionalLightColor;
    float directionalLightIntensity;
    float3 ambientLight;
    float padding1;
    float3 cameraPosition;
    float padding2;
};

cbuffer MaterialData : register(b2) {
    float3 materialAlbedo;
    float materialMetallic;
    float materialRoughness;
    float3 padding3;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

static const float PI = 3.14159265359f;

// Schlickの近似によるFresnel項
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

// GGX/Trowbridge-Reitz法線分布関数
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return num / denom;
}

// Schlick-GGX幾何減衰関数
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float num = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return num / denom;
}

// Smith's method幾何関数
float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float4 main(PSInput input) : SV_TARGET {
    // テクスチャから色を取得
    float3 albedo = albedoTexture.Sample(albedoSampler, input.uv).rgb;

    // 正規化されたベクトル
    float3 N = normalize(input.normal);
    float3 L = normalize(-directionalLightDirection);

    // Lambertライティング
    float NdotL = max(dot(N, L), 0.0f);
    float3 directLight = albedo * directionalLightColor * directionalLightIntensity * NdotL;

    // 環境光
    float3 ambient = albedo * ambientLight;

    // 最終的な色
    float3 color = ambient + directLight;

    return float4(color, 1.0f);
}
