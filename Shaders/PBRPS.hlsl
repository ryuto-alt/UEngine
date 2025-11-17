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

    // マテリアルパラメータを適用
    albedo *= materialAlbedo;
    float metallic = materialMetallic;
    float roughness = max(materialRoughness, 0.04f); // 完全に滑らかな面を避ける

    // 正規化されたベクトル
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPosition - input.worldPos);
    float3 L = normalize(-directionalLightDirection);
    float3 H = normalize(V + L);

    // F0の計算（金属性に応じて変化）
    float3 F0 = float3(0.04f, 0.04f, 0.04f);  // 非金属の基本反射率
    F0 = lerp(F0, albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    kD *= 1.0f - metallic;

    float3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
    float3 specular = numerator / denominator;

    // ディレクショナルライトの寄与
    float NdotL = max(dot(N, L), 0.0f);
    float3 radiance = directionalLightColor * directionalLightIntensity;
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // 環境光（シンプルなアンビエント）
    float3 ambient = ambientLight * albedo;

    // 最終的な色
    float3 color = ambient + Lo;

    // トーンマッピング（Reinhardトーンマッピング）
    color = color / (color + float3(1.0f, 1.0f, 1.0f));

    // ガンマ補正は、sRGBレンダーターゲットが自動で行います
    return float4(color, 1.0f);
}
