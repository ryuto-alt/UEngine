// Grass Pixel Shader - Godot風 Gradient Coloring + Wind Shadow + PBR Lighting

Texture2D    grassTexture   : register(t0);
Texture2D    shadowMap      : register(t1);
Texture2D    spotShadowMap0 : register(t2);
Texture2D    spotShadowMap1 : register(t3);
Texture2D    spotShadowMap2 : register(t4);
Texture2D    spotShadowMap3 : register(t5);
Texture2D    noiseTexture1  : register(t6);
Texture2D    noiseTexture2  : register(t7);
Texture2D    windNoiseTex   : register(t8);
SamplerState grassSampler   : register(s0);
SamplerComparisonState shadowSampler : register(s1);
SamplerState noiseSampler   : register(s2);

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

cbuffer GrassParams : register(b0) {
    // 変換行列（VS用、PSでは未使用だが同じCBなのでスキップ用に定義）
    matrix _view;
    matrix _projection;
    matrix _lightViewProj;
    matrix _spotLightViewProj[4];

    float3 _cameraPos;           float  _time;
    float3 bottomColor;          float  _windSpeed;
    float3 topColor;             float  _windDis;
    float3 colorVar1;            float  _noiseStrength;
    float3 colorVar2;            float  _displaceStrength;
    float3 windShadowColor;      float  windShadowStrength;
    float3 _interactingObjPos;   float  _flattenRadius;
    float  _flattenStrength;     float  _flattenFloor;
    float  noise1Scale;          float  noise2Scale;
    float  windNoiseScale;       float  _windNoisePanSpeedX;
    float  _windNoisePanSpeedY;  float  _noiseFloor;
    float  _windNoiseScaleStr;   float  _combinedNoiseMin;
    float  _combinedNoiseMax;    float  _invertNoise;
    float  windShadowDispThreshold; float windShadowSmoothing;
    float  flattenShadowStrength;   float _baseWidth;
    float  _baseHeight;          float  useGodotShading;
    float2 _cbPad;
};

cbuffer LightData : register(b1) {
    float3 directionalLightDirection; float padding0;
    float3 directionalLightColor;     float directionalLightIntensity;
    float3 ambientLight;              float padding1;
    float3 cameraPosition;            float padding2;
    GPUPointLight pointLights[8];
    int    pointLightCount;           float3 pad3;
    GPUSpotLight  spotLights[4];
    int    spotLightCount;            float3 pad4;
    float  shadowBias;
    int    spotShadowCount;           float2 shadowPad;
};

struct PSInput {
    float4 position       : SV_POSITION;
    float3 worldPos       : POSITION0;
    float3 normal         : NORMAL;
    float2 uv             : TEXCOORD0;
    float4 shadowPos      : TEXCOORD1;
    float4 spotShadowPos0 : TEXCOORD2;
    float4 spotShadowPos1 : TEXCOORD3;
    float4 spotShadowPos2 : TEXCOORD4;
    float4 spotShadowPos3 : TEXCOORD5;
    float  colorVariation : TEXCOORD6;
    float  windDisplacement : TEXCOORD7;
    float  flattenAmount    : TEXCOORD8;
    float  heightFactor     : TEXCOORD9;
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

float SampleSpotShadowPCF(Texture2D spotMap, float4 shadowPos) {
    float2 projUV = shadowPos.xy / shadowPos.w;
    float2 uv     = projUV * float2(0.5f, -0.5f) + 0.5f;
    float  depth  = shadowPos.z / shadowPos.w - shadowBias;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || depth < 0.0f || depth > 1.0f)
        return 1.0f;

    float shadow = 0.0f;
    float2 texelSize = 1.0f / 1024.0f;
    [unroll] for (int x = -1; x <= 1; x++) {
    [unroll] for (int y = -1; y <= 1; y++) {
        shadow += spotMap.SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * texelSize, depth);
    }}
    return shadow / 9.0f;
}

float4 main(PSInput input) : SV_TARGET {
    float4 texColor = grassTexture.Sample(grassSampler, input.uv);

    // アルファカットアウト
    if (texColor.a < 0.5f) {
        discard;
    }

    float3 albedo;

    if (useGodotShading > 0.5f) {
        // ===== GodotGrass: グラデーション + ノイズカラーリング =====
        float hf = input.heightFactor;

        // ベースグラデーション（先端→根元）
        float3 baseGradient = lerp(topColor, bottomColor, hf);

        // ノイズテクスチャでカラーバリエーション
        float2 nUV1 = input.worldPos.xz / max(noise1Scale, 0.01f);
        float2 nUV2 = input.worldPos.xz / max(noise2Scale, 0.01f);
        float blendVal1 = noiseTexture1.Sample(noiseSampler, nUV1).r;
        float blendVal2 = noiseTexture2.Sample(noiseSampler, nUV2).r;

        float3 altGradient1 = lerp(colorVar1, bottomColor, hf);
        float3 firstBlend = lerp(baseGradient, altGradient1, blendVal1);

        float3 altGradient2 = lerp(colorVar2, bottomColor, hf);
        float3 finalBlend = lerp(firstBlend, altGradient2, blendVal2);

        // 風ノイズで微妙な明暗
        float2 windUV = input.worldPos.xz / max(windNoiseScale, 0.01f);
        float noiseFactor = windNoiseTex.Sample(noiseSampler, windUV).r;
        finalBlend *= lerp(0.9f, 1.1f, noiseFactor);

        // テクスチャの輝度でディテール追加
        float texLuma = dot(texColor.rgb, float3(0.299f, 0.587f, 0.114f));
        albedo = finalBlend * lerp(0.8f, 1.2f, texLuma);

        // 風影（Wind Shadow）
        float displaceFade = smoothstep(
            windShadowDispThreshold,
            windShadowDispThreshold + windShadowSmoothing,
            input.windDisplacement
        );
        float flattenFade = smoothstep(0.0f, 1.0f, input.flattenAmount);
        float totalOverlayStrength = saturate(
            displaceFade * windShadowStrength +
            flattenFade * flattenShadowStrength
        );
        float3 shadowTint = lerp(float3(1.0f, 1.0f, 1.0f), windShadowColor, totalOverlayStrength);
        albedo = lerp(albedo, albedo * shadowTint, totalOverlayStrength);

    } else {
        // ===== 草原: テクスチャベースカラー =====
        float variation = lerp(0.8f, 1.2f, input.colorVariation);
        albedo = texColor.rgb * variation;
    }

    // ===== ライティング =====
    float3 N = normalize(input.normal);

    // カメラから見て裏面なら法線反転
    float3 viewDir = normalize(cameraPosition - input.worldPos);
    if (dot(N, viewDir) < 0.0f) {
        N = -N;
    }

    // ディレクショナルライト
    float3 L       = normalize(-directionalLightDirection);
    float  NdotL   = max(dot(N, L), 0.0f);
    // ラップライティング（半透明感）
    float  wrapNdotL = (NdotL + 0.5f) / 1.5f;
    float  shadow  = SampleShadowPCF(input.shadowPos);
    float3 directLight = albedo * directionalLightColor * directionalLightIntensity * wrapNdotL * shadow;

    // アンビエント（草は少し明るめに）
    float3 ambient = albedo * ambientLight * 1.2f;

    // ポイントライト
    float3 pointContrib = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < pointLightCount; i++) {
        float3 toLight = pointLights[i].position - input.worldPos;
        float  dist    = length(toLight);
        if (dist >= pointLights[i].range) continue;
        float  atten   = 1.0f - saturate(dist / pointLights[i].range);
        atten *= atten;
        float3 Lp      = normalize(toLight);
        float  NdotLp  = max(dot(N, Lp), 0.0f);
        pointContrib  += albedo * pointLights[i].color * pointLights[i].intensity * NdotLp * atten;
    }

    // スポットライト
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

        float spotShadow = 1.0f;
        if (j < spotShadowCount) {
            if      (j == 0) spotShadow = SampleSpotShadowPCF(spotShadowMap0, input.spotShadowPos0);
            else if (j == 1) spotShadow = SampleSpotShadowPCF(spotShadowMap1, input.spotShadowPos1);
            else if (j == 2) spotShadow = SampleSpotShadowPCF(spotShadowMap2, input.spotShadowPos2);
            else if (j == 3) spotShadow = SampleSpotShadowPCF(spotShadowMap3, input.spotShadowPos3);
        }

        spotContrib += albedo * spotLights[j].color * spotLights[j].intensity * NdotLs * atten * spotShadow;
    }

    float3 color = ambient + directLight + pointContrib + spotContrib;
    return float4(color, 1.0f);
}
