// VHS Tape Effect Pixel Shader - NTSC CRT style
// Inspired by shadertoy.com/view/3tVBWR
struct VSOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VHSParams {
    float time;
    float scanlineIntensity;
    float noiseIntensity;
    float trackingError;
    float chromaticAberration;   // controls chroma blur width
    float colorBleed;            // rightward chroma bias
    float sharpness;
    float tapeCrease;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<VHSParams> gParams : register(b0);

// ============================================================
// Constants
// ============================================================
#define PI 3.14159265
#define XRES 432.0
#define YRES 264.0
#define CHROMA_MOD_FREQ (0.4 * PI)

// ============================================================
// Utility
// ============================================================
float hash(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float smoothNoise(float2 st) {
    float2 i = floor(st);
    float2 f = frac(st);
    float a = hash(i);
    float b = hash(i + float2(1.0, 0.0));
    float c = hash(i + float2(0.0, 1.0));
    float d = hash(i + float2(1.0, 1.0));
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float peak(float x, float xpos, float scale) {
    return clamp((1.0 - x) * scale * log(1.0 / max(abs(x - xpos), 0.001)), 0.0, 1.0);
}

// ============================================================
// YIQ Color Space (NTSC)
// ============================================================
static const float3x3 RGB_TO_YIQ = float3x3(
    0.2990,  0.5870,  0.1140,
    0.5959, -0.2746, -0.3213,
    0.2115, -0.5227,  0.3112
);
static const float3x3 YIQ_TO_RGB = float3x3(
    1.0,  0.956,  0.621,
    1.0, -0.272, -0.647,
    1.0, -1.106,  1.703
);

float3 rgb2yiq(float3 c) { return mul(RGB_TO_YIQ, c); }
float3 yiq2rgb(float3 c) { return mul(YIQ_TO_RGB, c); }

// ============================================================
// Multi-frequency tape wobble
// ============================================================
float tapeWobble(float y, float t) {
    float wobble = sin(y * 1.5 + t * 0.4) * 0.003;
    wobble += sin(y * 8.0 + t * 1.7) * 0.001;
    wobble += sin(y * 40.0 + t * 15.0) * 0.0003;
    wobble += (hash(float2(floor(y * 480.0), floor(t * 30.0))) - 0.5) * 0.0008;
    return wobble;
}

// ============================================================
// Main
// ============================================================
float4 main(VSOutput input) : SV_TARGET {
    float2 uv = input.texcoord;
    float t = gParams.time;
    float timeSeed = floor(t * 30.0);
    float scany = round(uv.y * YRES);

    // --- Resolution reduction ---
    if (gParams.sharpness < 1.0) {
        float pixelSize = lerp(0.008, 0.001, gParams.sharpness);
        uv = floor(uv / pixelSize) * pixelSize;
    }

    // --- Scan flicker (interlace-like vertical jitter) ---
    float mframe = floor(frac(t * 30.0) * 2.0); // alternates 0/1
    uv.y += mframe * (1.0 / YRES) * 0.33 * gParams.scanlineIntensity;

    // --- Multi-frequency tape wobble ---
    uv.x += tapeWobble(uv.y, t) * gParams.trackingError;

    // --- Per-scanline interference (random horizontal displacement) ---
    {
        float r = hash(float2(timeSeed, scany));
        if (r > 0.995) { r *= 3.0; }
        float ifx1 = gParams.trackingError * 2.0 / XRES * r;
        float ifx2 = gParams.trackingError * 0.001 * (r * peak(uv.y, 0.2, 0.2));
        uv.x += ifx1 - ifx2;
    }

    // --- Tape crease (drifting horizontal distortion band) ---
    float creasePos = frac(t * 0.08);
    float creaseDist = abs(uv.y - creasePos);
    if (creaseDist < 0.04) {
        float ci = (0.04 - creaseDist) * 25.0;
        uv.x += sin(uv.y * 80.0) * ci * gParams.tapeCrease * 0.015;
    }

    // --- YIQ chroma subsampling ---
    float texelX = 1.0 / 1280.0;
    float blurRadius = gParams.chromaticAberration * 1.5;
    float rightBias = gParams.colorBleed * 0.5;

    float Y = dot(gTexture.Sample(gSampler, uv).rgb, float3(0.2990, 0.5870, 0.1140));
    float I = 0.0;
    float Q = 0.0;
    float totalW = 0.0;

    for (int i = -6; i <= 6; i++) {
        float fi = float(i) + rightBias;
        float weight = exp(-0.5 * fi * fi / max(blurRadius * blurRadius, 0.01));
        float2 sampleUV = uv + float2(float(i) * texelX * blurRadius, 0.0);
        float3 yiq = rgb2yiq(gTexture.Sample(gSampler, sampleUV).rgb);
        I += yiq.y * weight;
        Q += yiq.z * weight;
        totalW += weight;
    }
    I /= totalW;
    Q /= totalW;

    // --- Luma / chroma noise ---
    float lumaNoise = (hash(uv * float2(640.0, 480.0) + timeSeed) - 0.5);
    Y += lumaNoise * gParams.noiseIntensity * 0.08;

    float2 chromaNoiseUV = floor(uv * float2(160.0, 240.0)) / float2(160.0, 240.0);
    I += (hash(chromaNoiseUV + timeSeed + 100.0) - 0.5) * gParams.noiseIntensity * 0.15;
    Q += (hash(chromaNoiseUV + timeSeed + 200.0) - 0.5) * gParams.noiseIntensity * 0.15;

    float3 color = yiq2rgb(float3(Y, I, Q));

    // --- Edge ringing / overshoot ---
    {
        float3 left  = gTexture.Sample(gSampler, uv - float2(texelX, 0.0)).rgb;
        float3 right = gTexture.Sample(gSampler, uv + float2(texelX, 0.0)).rgb;
        float3 center = gTexture.Sample(gSampler, uv).rgb;
        color -= (left + right - 2.0 * center) * 0.12;
    }

    // --- Scanlines (NTSC CRT style) ---
    {
        float scanl = 0.5 + 0.5 * abs(sin(PI * uv.y * YRES));
        color *= lerp(1.0, scanl, gParams.scanlineIntensity);
    }

    // --- Rolling tracking error bands (scrolls bottom to top) ---
    // Multiple bands with horizontal displacement, brightness distortion, chromatic split
    {
        float trackStr = gParams.trackingError;

        // Band 1: main wide band (Cauchy window, ~10% screen height)
        float scrollPos1 = frac(-t * 0.15);
        float dist1 = uv.y - scrollPos1;
        if (dist1 > 0.5) dist1 -= 1.0;
        if (dist1 < -0.5) dist1 += 1.0;
        float band1 = 1.0 / (1.0 + 60.0 * dist1 * dist1);

        // Band 2: secondary thinner band at different speed
        float scrollPos2 = frac(-t * 0.23 + 0.4);
        float dist2 = uv.y - scrollPos2;
        if (dist2 > 0.5) dist2 -= 1.0;
        if (dist2 < -0.5) dist2 += 1.0;
        float band2 = 1.0 / (1.0 + 120.0 * dist2 * dist2);

        // Band 3: subtle fast band
        float scrollPos3 = frac(-t * 0.37 + 0.7);
        float dist3 = uv.y - scrollPos3;
        if (dist3 > 0.5) dist3 -= 1.0;
        if (dist3 < -0.5) dist3 += 1.0;
        float band3 = 1.0 / (1.0 + 200.0 * dist3 * dist3);

        float totalBand = saturate(band1 + band2 * 0.6 + band3 * 0.3);

        // Horizontal displacement within bands (straight tear)
        float hDisplace = totalBand * trackStr * 0.015;

        // Re-sample with displacement + per-channel chromatic split
        float2 tearUV = float2(uv.x + hDisplace, uv.y);
        float convergence = totalBand * trackStr * 0.008;
        float rr = gTexture.Sample(gSampler, tearUV + float2(convergence, 0.0)).r;
        float gg = gTexture.Sample(gSampler, tearUV).g;
        float bb = gTexture.Sample(gSampler, tearUV - float2(convergence, 0.0)).b;
        float3 tornColor = float3(rr, gg, bb);

        // Blend torn color into output within band region
        color = lerp(color, tornColor, totalBand * trackStr * 3.0);

        // Slight brighten + desaturate within band (signal degradation)
        float bandLum = dot(color, float3(0.299, 0.587, 0.114));
        float3 bandGray = float3(bandLum, bandLum, bandLum);
        color = lerp(color, bandGray * 1.15, totalBand * trackStr * 0.4);

        // Subtle noise within band
        float bandNoise = (hash(uv * float2(640.0, 480.0) + t * 7.0) - 0.5) * totalBand * trackStr * 0.06;
        color += bandNoise;
    }

    // --- Color desaturation ---
    float gray = dot(color, float3(0.299, 0.587, 0.114));
    color = lerp(color, float3(gray, gray, gray), 0.1);

    // --- Slight contrast ---
    color = pow(max(color, 0.0), float3(1.05, 1.05, 1.05)) * 0.97;

    // --- Top / bottom edge degradation ---
    float edgeFade = smoothstep(0.0, 0.04, min(input.texcoord.y, 1.0 - input.texcoord.y));
    color *= edgeFade;

    return float4(saturate(color), 1.0);
}
