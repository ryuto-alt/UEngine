// Grass Vertex Shader - Godot風 Noise Wind + Flattening + Height Variation

Texture2D    noiseTexture1 : register(t6);
Texture2D    noiseTexture2 : register(t7);
Texture2D    windNoiseTex  : register(t8);
SamplerState noiseSampler  : register(s2);

cbuffer GrassTransform : register(b0) {
    // 変換行列
    matrix view;
    matrix projection;
    matrix lightViewProj;
    matrix spotLightViewProj[4];

    // カメラ & 時間
    float3 cameraPos;            float  time;

    // カラー
    float3 bottomColor;          float  windSpeed;
    float3 topColor;             float  windDis;
    float3 colorVar1;            float  noiseStrength;
    float3 colorVar2;            float  displaceStrength;

    // 風影 & インタラクション
    float3 windShadowColor;      float  windShadowStrength;
    float3 interactingObjPos;    float  flattenRadius;
    float  flattenStrength;      float  flattenFloor;
    float  noise1Scale;          float  noise2Scale;

    // ノイズ & スケール
    float  windNoiseScale;       float  windNoisePanSpeedX;
    float  windNoisePanSpeedY;   float  noiseFloor;
    float  windNoiseScaleStrength; float combinedNoiseMinScale;
    float  combinedNoiseMaxScale;  float invertCombinedNoise;
    float  windShadowDispThreshold; float windShadowSmoothing;
    float  flattenShadowStrength;   float baseWidth;

    float  baseHeight;           float  useGodotShading;
    float2 cbPadding;
};

struct VSInput {
    // Per-vertex (Slot 0)
    float3 position     : POSITION;
    float2 uv           : TEXCOORD;
    float  heightFactor : HEIGHT_FACTOR;

    // Per-instance (Slot 1)
    float3 instancePos  : INSTANCE_POS;
    float3 instanceData : INSTANCE_DATA;  // x=rotation, y=scale, z=colorVariation
};

struct VSOutput {
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
    float  heightFactorOut  : TEXCOORD9;
};

VSOutput main(VSInput input) {
    VSOutput output;

    float rotation = input.instanceData.x;
    float scale    = input.instanceData.y;
    float colorVar = input.instanceData.z;

    // Y軸回転 + ベースサイズ適用
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    float3 localPos = input.position;
    localPos.xz *= baseWidth;
    localPos.y  *= baseHeight;
    float3 rotatedPos;
    rotatedPos.x = localPos.x * cosR - localPos.z * sinR;
    rotatedPos.y = localPos.y * scale;
    rotatedPos.z = localPos.x * sinR + localPos.z * cosR;

    // 初期ワールド座標（ノイズサンプリング用）
    float3 worldPos = rotatedPos + input.instancePos;
    float3 instanceOrigin = input.instancePos;

    // ===== ノイズベースの高さスケーリング =====
    float2 uv1 = worldPos.xz / max(noise1Scale, 0.01f);
    float2 uv2 = worldPos.xz / max(noise2Scale, 0.01f);
    float noise1Val = noiseTexture1.SampleLevel(noiseSampler, uv1, 0).r;
    float noise2Val = noiseTexture2.SampleLevel(noiseSampler, uv2, 0).r;
    float combinedNoise = saturate((0.5f + noise1Val) * noise2Val);
    if (invertCombinedNoise > 0.5f) {
        combinedNoise = 1.0f - combinedNoise;
    }

    // 風ノイズでの高さ変動
    float2 windNoiseUV = instanceOrigin.xz / max(windNoiseScale, 0.01f)
                       + float2(windNoisePanSpeedX, windNoisePanSpeedY) * time;
    float windNoiseVal = windNoiseTex.SampleLevel(noiseSampler, windNoiseUV, 0).r;
    float windVerticalOffset = lerp(-windNoiseScaleStrength, windNoiseScaleStrength, windNoiseVal);

    // 最終的な高さスケール
    float verticalScaleNoise = lerp(combinedNoiseMinScale, combinedNoiseMaxScale, 1.0f - combinedNoise);
    float finalVerticalScale = verticalScaleNoise + windVerticalOffset;

    // ===== 踏み倒し（フラッテニング） =====
    float distToImpact = distance(worldPos, interactingObjPos);
    float flattenFactor = saturate(1.0f - (distToImpact / max(flattenRadius, 0.001f)));
    float flattenAmount = flattenFactor * flattenStrength;
    float flattenScale = lerp(1.0f, flattenFloor, saturate(flattenAmount));
    finalVerticalScale *= flattenScale;

    // ベースからの高さにスケール適用
    float heightAboveBase = worldPos.y - instanceOrigin.y;
    worldPos.y = instanceOrigin.y + heightAboveBase * finalVerticalScale;

    // ===== 風アニメーション =====
    float bias = windNoiseVal - 0.5f;
    float shapedNoise;
    if (bias < 0.0f) {
        shapedNoise = lerp(0.0f, noiseFloor - 0.5f, -bias * 2.0f);
    } else {
        shapedNoise = bias * bias * 2.0f;
    }

    float instanceWindSpeed = max(0.0f, windSpeed * (1.0f - shapedNoise * noiseStrength));
    float windResponse = 1.0f - saturate(flattenAmount);
    float instanceWindDis = windDis * (1.0f + shapedNoise * displaceStrength) * windResponse;

    float sineValue = sin(time * instanceWindSpeed);

    // 風向き（パン速度から導出）
    float2 windPan = float2(windNoisePanSpeedX, windNoisePanSpeedY);
    float2 windDir = (length(windPan) > 0.001f) ? normalize(windPan) : float2(1.0f, 0.0f);

    // 先端ほど大きく揺れる（heightFactor: 0=根元, 1=先端）
    float2 sway = windDir * sineValue * instanceWindDis * input.heightFactor;
    worldPos.x += sway.x;
    worldPos.z += sway.y;

    // 重力的な先端の垂れ下がり
    worldPos.y -= input.heightFactor * input.heightFactor * instanceWindDis * abs(sineValue) * 0.1f;

    // ===== 出力 =====
    output.worldPos = worldPos;
    output.windDisplacement = instanceWindDis;
    output.flattenAmount = saturate(flattenAmount);
    output.heightFactorOut = input.heightFactor;

    // ビュー・プロジェクション
    float4 viewPos = mul(float4(worldPos, 1.0f), view);
    output.position = mul(viewPos, projection);

    // 法線（草は常に上向き + 少しランダム）
    output.normal = normalize(float3(sinR * 0.3f, 1.0f, cosR * 0.3f));

    output.uv = input.uv;
    output.colorVariation = colorVar;

    // シャドウ座標
    float4 wp4 = float4(worldPos, 1.0f);
    output.shadowPos      = mul(wp4, lightViewProj);
    output.spotShadowPos0 = mul(wp4, spotLightViewProj[0]);
    output.spotShadowPos1 = mul(wp4, spotLightViewProj[1]);
    output.spotShadowPos2 = mul(wp4, spotLightViewProj[2]);
    output.spotShadowPos3 = mul(wp4, spotLightViewProj[3]);

    return output;
}
