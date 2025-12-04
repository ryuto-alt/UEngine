// ParticleProceduralPS.hlsl - Procedural Shape Particle Pixel Shader
// SDFベースのプロシージャル形状描画

#define PI 3.14159265359
#define TWO_PI 6.28318530718

// 定数バッファ
cbuffer ParticleRenderCB : register(b2) {
    uint useTexture;        // テクスチャ使用フラグ
    uint blendMode;         // ブレンドモード
    float softParticleScale;
    uint proceduralShape;   // プロシージャル形状タイプ
    float proceduralParam1; // パラメータ1
    float proceduralParam2; // パラメータ2
    float totalTime;        // 時間（アニメーション用）
    float padding;
};

// ブレンドモード定数
#define BLEND_ADDITIVE      0
#define BLEND_ALPHA         1
#define BLEND_MULTIPLY      2
#define BLEND_PREMULTIPLIED 3

// プロシージャル形状定数
#define SHAPE_NONE          0
#define SHAPE_CIRCLE        1
#define SHAPE_RING          2
#define SHAPE_STAR          3
#define SHAPE_PENTAGON      4
#define SHAPE_HEXAGON       5
#define SHAPE_MAGIC_CIRCLE  6
#define SHAPE_RUNE          7
#define SHAPE_CROSS         8
#define SHAPE_SPARKLE       9

// 入力構造体
struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float lifetime : TEXCOORD1;
};

// ================================
// SDF関数（2D Signed Distance Functions）
// ================================

// 円のSDF
float sdCircle(float2 p, float r) {
    return length(p) - r;
}

// リングのSDF
float sdRing(float2 p, float outerR, float innerR) {
    return abs(length(p) - (outerR + innerR) * 0.5) - (outerR - innerR) * 0.5;
}

// 多角形のSDF
float sdPolygon(float2 p, int n, float r) {
    float an = PI / float(n);
    float bn = atan2(p.x, p.y);
    bn = abs(fmod(bn, 2.0 * an) - an);
    p = length(p) * float2(cos(bn), sin(bn));
    p -= r * float2(cos(an), sin(an));
    p.y = max(p.y, 0.0);
    return length(p) * sign(p.x);
}

// 星形のSDF
float sdStar(float2 p, int n, float outerR, float innerR) {
    float an = PI / float(n);
    float bn = atan2(p.x, p.y);
    bn = fmod(bn + an, 2.0 * an) - an;
    p = length(p) * float2(cos(bn), abs(sin(bn)));

    float2 k1 = float2(outerR, 0.0);
    float2 k2 = float2(innerR * cos(an), innerR * sin(an));
    float2 e = k2 - k1;
    float2 w = p - k1;

    float2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
    return length(b) * sign(e.y * w.x - e.x * w.y);
}

// 十字のSDF
float sdCross(float2 p, float2 size, float r) {
    p = abs(p);
    p = (p.y > p.x) ? p.yx : p.xy;
    float2 q = p - size;
    float k = max(q.y, q.x);
    float2 w = (k > 0.0) ? q : float2(size.y - p.x, -k);
    return sign(k) * length(max(w, 0.0)) - r;
}

// グロー効果
float glow(float d, float intensity, float falloff) {
    return intensity / (1.0 + pow(abs(d) * falloff, 2.0));
}

// スムーズステップ境界
float smoothBorder(float d, float width) {
    return 1.0 - smoothstep(0.0, width, d);
}

// ================================
// 複合形状関数
// ================================

// 魔法陣を描画
float4 drawMagicCircle(float2 uv, float4 baseColor, float time, float param1, float param2) {
    float2 p = uv * 2.0 - 1.0;  // -1 to 1に正規化

    float alpha = 0.0;
    float3 col = baseColor.rgb;

    // 回転
    float angle1 = time * 0.5;
    float angle2 = -time * 0.8;
    float angle3 = time * 0.3;

    float2x2 rot1 = float2x2(cos(angle1), -sin(angle1), sin(angle1), cos(angle1));
    float2x2 rot2 = float2x2(cos(angle2), -sin(angle2), sin(angle2), cos(angle2));
    float2x2 rot3 = float2x2(cos(angle3), -sin(angle3), sin(angle3), cos(angle3));

    // 1. 外側リング
    float outerRing = sdRing(p, 0.95, 0.85);
    alpha += smoothBorder(outerRing, 0.02) * 0.8;
    alpha += glow(outerRing, 0.3, 20.0);

    // 2. 外側リング装飾（回転するドット）
    float2 p1 = mul(rot1, p);
    int numDots = int(param2);
    for (int i = 0; i < 12; i++) {
        if (i >= numDots) break;
        float a = float(i) / float(numDots) * TWO_PI;
        float2 dotPos = float2(cos(a), sin(a)) * 0.9;
        float d = sdCircle(p1 - dotPos, 0.03);
        alpha += smoothBorder(d, 0.01) * 0.9;
        alpha += glow(d, 0.15, 40.0);
    }

    // 3. 中間リング
    float midRing = sdRing(p, 0.75, 0.70);
    alpha += smoothBorder(midRing, 0.015) * 0.7;
    alpha += glow(midRing, 0.2, 25.0);

    // 4. 内側六芒星（回転）
    float2 p2 = mul(rot2, p);
    float star1 = sdStar(p2, 6, 0.6, 0.3);
    alpha += smoothBorder(star1, 0.02) * 0.6;
    alpha += glow(star1, 0.25, 15.0);

    // 5. 内側五芒星（逆回転）
    float2 p3 = mul(rot3, p);
    float star2 = sdStar(p3, 5, 0.45, 0.2);
    alpha += smoothBorder(star2, 0.015) * 0.5;
    alpha += glow(star2, 0.2, 18.0);

    // 6. 中心円
    float centerCircle = sdCircle(p, 0.15);
    alpha += smoothBorder(centerCircle, 0.02) * 0.8;
    alpha += glow(centerCircle, 0.4, 10.0);

    // 7. 中心グロー
    float centerGlow = length(p);
    alpha += glow(centerGlow, 0.5, 5.0);

    // 8. 放射状ライン
    float lineAngle = atan2(p.y, p.x) + angle1;
    float numLines = 8.0;
    float linePattern = abs(frac(lineAngle / TWO_PI * numLines) - 0.5) * 2.0;
    float lineDist = length(p);
    if (lineDist > 0.2 && lineDist < 0.65) {
        alpha += (1.0 - linePattern) * 0.15 * smoothstep(0.2, 0.3, lineDist) * smoothstep(0.65, 0.55, lineDist);
    }

    // カラー調整（中心に向かって明るく）
    float3 finalColor = col * (1.0 + glow(length(p), 0.3, 3.0));

    alpha = saturate(alpha);
    return float4(finalColor, alpha * baseColor.a);
}

// ルーン文字風
float4 drawRune(float2 uv, float4 baseColor, float time, float param1) {
    float2 p = uv * 2.0 - 1.0;
    float alpha = 0.0;

    // 基本形状：縦線
    float vline = abs(p.x);
    alpha += smoothBorder(vline - 0.05, 0.02) * 0.8;

    // 斜め線
    float2 p2 = p;
    p2.y += 0.3;
    float dline = abs(p2.x - p2.y * 0.5);
    if (p.y > -0.3 && p.y < 0.3) {
        alpha += smoothBorder(dline - 0.04, 0.02) * 0.7;
    }

    // 横線
    if (abs(p.y - 0.3) < 0.05 && abs(p.x) < 0.3) {
        alpha += 0.6;
    }

    // グロー
    alpha += glow(length(p), 0.2, 8.0);

    // パルスアニメーション
    float pulse = 0.8 + 0.2 * sin(time * 3.0);

    return float4(baseColor.rgb * pulse, saturate(alpha) * baseColor.a);
}

// きらめきエフェクト
float4 drawSparkle(float2 uv, float4 baseColor, float time) {
    float2 p = uv * 2.0 - 1.0;
    float alpha = 0.0;

    // 4方向のスパイク
    float spike1 = abs(p.x) + abs(p.y);  // ダイヤモンド形
    float spike2 = max(abs(p.x), abs(p.y));  // 四角形

    float d = min(spike1, spike2 * 1.5);
    alpha = glow(d, 1.0, 3.0);

    // 中心の強い光
    alpha += glow(length(p), 0.8, 8.0);

    // パルス
    float pulse = 0.7 + 0.3 * sin(time * 5.0);

    return float4(baseColor.rgb, saturate(alpha * pulse) * baseColor.a);
}

// ================================
// メインピクセルシェーダー
// ================================

float4 main(PSInput input) : SV_TARGET {
    float4 finalColor;

    switch (proceduralShape) {
        case SHAPE_CIRCLE: {
            float2 p = input.texCoord * 2.0 - 1.0;
            float d = sdCircle(p, 0.9);
            float alpha = smoothBorder(d, 0.05);
            alpha += glow(d, 0.3, 10.0);
            finalColor = float4(input.color.rgb, saturate(alpha) * input.color.a);
            break;
        }

        case SHAPE_RING: {
            float2 p = input.texCoord * 2.0 - 1.0;
            float d = sdRing(p, 0.9, 0.9 - proceduralParam1);
            float alpha = smoothBorder(d, 0.03);
            alpha += glow(d, 0.4, 15.0);
            finalColor = float4(input.color.rgb, saturate(alpha) * input.color.a);
            break;
        }

        case SHAPE_STAR: {
            float2 p = input.texCoord * 2.0 - 1.0;
            int points = int(proceduralParam2);
            float d = sdStar(p, points, 0.9, 0.9 * proceduralParam1);
            float alpha = smoothBorder(d, 0.03);
            alpha += glow(d, 0.3, 12.0);
            finalColor = float4(input.color.rgb, saturate(alpha) * input.color.a);
            break;
        }

        case SHAPE_PENTAGON: {
            float2 p = input.texCoord * 2.0 - 1.0;
            float d = sdPolygon(p, 5, 0.85);
            float alpha = smoothBorder(d, 0.03);
            alpha += glow(d, 0.25, 12.0);
            finalColor = float4(input.color.rgb, saturate(alpha) * input.color.a);
            break;
        }

        case SHAPE_HEXAGON: {
            float2 p = input.texCoord * 2.0 - 1.0;
            float d = sdPolygon(p, 6, 0.85);
            float alpha = smoothBorder(d, 0.03);
            alpha += glow(d, 0.25, 12.0);
            finalColor = float4(input.color.rgb, saturate(alpha) * input.color.a);
            break;
        }

        case SHAPE_MAGIC_CIRCLE: {
            finalColor = drawMagicCircle(input.texCoord, input.color, totalTime, proceduralParam1, proceduralParam2);
            break;
        }

        case SHAPE_RUNE: {
            finalColor = drawRune(input.texCoord, input.color, totalTime, proceduralParam1);
            break;
        }

        case SHAPE_CROSS: {
            float2 p = input.texCoord * 2.0 - 1.0;
            float d = sdCross(p, float2(0.8, 0.15), 0.02);
            float alpha = smoothBorder(d, 0.03);
            alpha += glow(d, 0.3, 12.0);
            finalColor = float4(input.color.rgb, saturate(alpha) * input.color.a);
            break;
        }

        case SHAPE_SPARKLE: {
            finalColor = drawSparkle(input.texCoord, input.color, totalTime);
            break;
        }

        default: {
            // デフォルト：円形グラデーション
            float2 p = input.texCoord * 2.0 - 1.0;
            float d = length(p);
            float alpha = 1.0 - smoothstep(0.0, 1.0, d);
            alpha += glow(d, 0.2, 5.0);
            finalColor = float4(input.color.rgb, saturate(alpha) * input.color.a);
            break;
        }
    }

    // ブレンドモードによる調整
    switch (blendMode) {
        case BLEND_ADDITIVE:
            finalColor.rgb *= finalColor.a;
            finalColor.a = 1.0f;
            break;
        case BLEND_ALPHA:
            break;
        case BLEND_MULTIPLY:
            break;
        case BLEND_PREMULTIPLIED:
            break;
    }

    // アルファが0に近い場合は破棄
    if (finalColor.a < 0.001f) {
        discard;
    }

    return finalColor;
}
