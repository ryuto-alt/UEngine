// Skinned Pixel Shader

Texture2D    diffuseTexture : register(t1);
Texture2D    shadowMap      : register(t2);
SamplerState samplerState   : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct GPUPointLight {
    float3 position;  float range;
    float3 color;     float intensity;
};

struct GPUSpotLight {
    float3 position;  float range;
    float3 direction; float spotAngle;
    float3 color;     float intensity;
    float  innerAngle; float3 pad;
};

cbuffer Light : register(b1) {
    float3 directionalLightDirection; float padding0;
    float3 directionalLightColor;     float directionalLightIntensity;
    float3 ambientLight;              float padding1;
    float3 cameraPosition;            float padding2;
    GPUPointLight pointLights[8];
    int    pointLightCount;           float3 pad3;
    GPUSpotLight  spotLights[4];
    int    spotLightCount;            float3 pad4;
    float  shadowBias;                float3 shadowPad;
};

cbuffer Material : register(b2) {
    float3 albedo;
    float  metallic;
    float  roughness;
    float3 padding3;
};

struct PSInput {
    float4 position  : SV_POSITION;
    float3 worldPos  : POSITION0;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    float4 shadowPos : TEXCOORD1;
};

float SampleShadowPCF(float4 shadowPos) {
    float2 projUV = shadowPos.xy / shadowPos.w;
    float2 uv     = projUV * float2(0.5f, -0.5f) + 0.5f;
    float  depth  = shadowPos.z / shadowPos.w - shadowBias;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || depth < 0.0f || depth > 1.0f)
        return 1.0f;

    float shadow = 0.0f;
    float2 texelSize = 1.0f / 2048.0f;
    [unroll] for (int x = -1; x <= 1; x++) {
    [unroll] for (int y = -1; y <= 1; y++) {
        shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * texelSize, depth);
    }}
    return shadow / 9.0f;
}

float4 main(PSInput input) : SV_TARGET {
    float4 texColor = diffuseTexture.Sample(samplerState, input.uv);

    float3 N = normalize(input.normal);
    float3 L = normalize(-directionalLightDirection);

    float  NdotL       = max(dot(N, L), 0.0f);
    float  shadow      = SampleShadowPCF(input.shadowPos);
    float3 directLight = texColor.rgb * albedo * directionalLightColor * directionalLightIntensity * NdotL * shadow;

    float3 ambient = texColor.rgb * albedo * ambientLight;

    // Point lights
    float3 pointContrib = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < pointLightCount; i++) {
        float3 toLight = pointLights[i].position - input.worldPos;
        float  dist    = length(toLight);
        if (dist >= pointLights[i].range) continue;
        float  atten   = 1.0f - saturate(dist / pointLights[i].range);
        atten *= atten;
        float3 Lp      = normalize(toLight);
        float  NdotLp  = max(dot(N, Lp), 0.0f);
        pointContrib  += texColor.rgb * albedo * pointLights[i].color * pointLights[i].intensity * NdotLp * atten;
    }

    // Spot lights
    float3 spotContrib = float3(0.0f, 0.0f, 0.0f);
    for (int j = 0; j < spotLightCount; j++) {
        float3 toLight  = spotLights[j].position - input.worldPos;
        float  dist     = length(toLight);
        if (dist >= spotLights[j].range) continue;
        float3 Ls       = normalize(toLight);
        float  cosAngle = dot(-Ls, normalize(spotLights[j].direction));
        float  cosOuter = cos(spotLights[j].spotAngle);
        float  cosInner = cos(spotLights[j].innerAngle);
        float  spotF    = saturate((cosAngle - cosOuter) / max(cosInner - cosOuter, 0.0001f));
        float  atten    = (1.0f - saturate(dist / spotLights[j].range));
        atten *= atten * spotF;
        float  NdotLs   = max(dot(N, Ls), 0.0f);
        spotContrib    += texColor.rgb * albedo * spotLights[j].color * spotLights[j].intensity * NdotLs * atten;
    }

    float3 finalColor = ambient + directLight + pointContrib + spotContrib;
    return float4(finalColor, texColor.a);
}
