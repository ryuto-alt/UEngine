// VHS Tape Effect Pixel Shader - Realistic YIQ-based
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
    float wobble = sin(y * 1.5 + t * 0.4) * 0.003;         // capstan
    wobble += sin(y * 8.0 + t * 1.7) * 0.001;              // tension
    wobble += sin(y * 40.0 + t * 15.0) * 0.0003;           // head drum
    wobble += (hash(float2(floor(y * 480.0), floor(t * 30.0))) - 0.5) * 0.0008; // jitter
    return wobble;
}

// ============================================================
// Main
// ============================================================
float4 main(VSOutput input) : SV_TARGET {
    float2 uv = input.texcoord;
    float t = gParams.time;
    float timeSeed = floor(t * 30.0);

    // --- Resolution reduction (pixelate first) ---
    if (gParams.sharpness < 1.0) {
        float pixelSize = lerp(0.008, 0.001, gParams.sharpness);
        uv = floor(uv / pixelSize) * pixelSize;
    }

    // --- Multi-frequency tape wobble ---
    uv.x += tapeWobble(uv.y, t) * gParams.trackingError;

    // --- Per-scanline tracking jitter ---
    float trackNoise = smoothNoise(float2(timeSeed, uv.y * 40.0));
    uv.x += (trackNoise - 0.5) * gParams.trackingError * 0.008;

    // --- Tape crease (drifting horizontal distortion band) ---
    float creasePos = frac(t * 0.08);
    float creaseDist = abs(uv.y - creasePos);
    if (creaseDist < 0.04) {
        float ci = (0.04 - creaseDist) * 25.0;
        uv.x += sin(uv.y * 80.0) * ci * gParams.tapeCrease * 0.015;
    }

    // --- Head-switching noise (bottom ~5% of frame) ---
    float headSwitch = smoothstep(0.92, 0.98, uv.y);
    if (headSwitch > 0.0) {
        uv.x += headSwitch * sin(uv.y * 200.0 + t * 50.0) * 0.06;
    }

    // --- YIQ chroma subsampling (the core of realistic VHS) ---
    // Sample luminance at full resolution
    float Y = dot(gTexture.Sample(gSampler, uv).rgb, float3(0.2990, 0.5870, 0.1140));

    // Blur I and Q channels horizontally (VHS chroma bandwidth ~0.5MHz vs 3MHz luma)
    float I = 0.0;
    float Q = 0.0;
    float totalW = 0.0;
    float blurRadius = gParams.chromaticAberration * 1.5; // controls blur width
    float texelX = 1.0 / 1280.0;
    float rightBias = gParams.colorBleed * 0.5; // asymmetric rightward smear

    for (int i = -6; i <= 6; i++) {
        float fi = float(i) + rightBias; // shift kernel rightward
        float weight = exp(-0.5 * fi * fi / max(blurRadius * blurRadius, 0.01));
        float2 sampleUV = uv + float2(float(i) * texelX * blurRadius, 0.0);
        float3 yiq = rgb2yiq(gTexture.Sample(gSampler, sampleUV).rgb);
        I += yiq.y * weight;
        Q += yiq.z * weight;
        totalW += weight;
    }
    I /= totalW;
    Q /= totalW;

    // --- Separate luma / chroma noise (YIQ space) ---
    // Luma noise: fine grain
    float lumaNoise = (hash(uv * float2(640.0, 480.0) + timeSeed) - 0.5);
    Y += lumaNoise * gParams.noiseIntensity * 0.08;

    // Chroma noise: coarser, blockier (lower resolution sampling)
    float2 chromaNoiseUV = floor(uv * float2(160.0, 240.0)) / float2(160.0, 240.0);
    float chromaNoiseI = (hash(chromaNoiseUV + timeSeed + 100.0) - 0.5);
    float chromaNoiseQ = (hash(chromaNoiseUV + timeSeed + 200.0) - 0.5);
    I += chromaNoiseI * gParams.noiseIntensity * 0.15;
    Q += chromaNoiseQ * gParams.noiseIntensity * 0.15;

    // Convert back to RGB
    float3 color = yiq2rgb(float3(Y, I, Q));

    // --- Edge ringing / overshoot (analog sharpening artifact) ---
    {
        float3 left  = gTexture.Sample(gSampler, uv - float2(texelX, 0.0)).rgb;
        float3 right = gTexture.Sample(gSampler, uv + float2(texelX, 0.0)).rgb;
        float3 center = gTexture.Sample(gSampler, uv).rgb;
        float3 laplacian = left + right - 2.0 * center;
        color -= laplacian * 0.12;
    }

    // --- Head-switching noise: brightness corruption at bottom ---
    if (headSwitch > 0.0) {
        float hsNoise = hash(float2(floor(uv.x * 320.0), timeSeed)) * headSwitch;
        color = lerp(color, float3(hsNoise, hsNoise * 0.8, hsNoise * 0.6), headSwitch * 0.7);
    }

    // --- Occasional horizontal noise lines ---
    float lineNoise = hash(float2(timeSeed, floor(input.texcoord.y * 240.0)));
    if (lineNoise > 0.985) {
        color += gParams.noiseIntensity * 0.25;
    }

    // --- Tape dropout (rare white streaks) ---
    float dropLine = hash(float2(timeSeed + 50.0, floor(uv.y * 480.0)));
    if (dropLine > 0.998) {
        float startX = hash(float2(timeSeed + 51.0, floor(uv.y * 480.0)));
        float len = hash(float2(timeSeed + 52.0, floor(uv.y * 480.0))) * 0.25 + 0.05;
        if (uv.x > startX && uv.x < startX + len) {
            float dn = hash(uv * 1000.0 + timeSeed) * 0.3 + 0.7;
            color = float3(dn, dn, dn);
        }
    }

    // --- Gaussian beam scanlines ---
    {
        float scanPos = input.texcoord.y * 240.0; // ~240 visible scanlines (NTSC field)
        float scanFrac = frac(scanPos);
        // Gaussian beam profile: bright pixels bloom wider
        float lum = dot(color, float3(0.299, 0.587, 0.114));
        float beamWidth = lerp(10.0, 5.0, saturate(lum));
        float beam = exp(-beamWidth * (scanFrac - 0.5) * (scanFrac - 0.5));
        color *= lerp(1.0, beam, gParams.scanlineIntensity * 0.5);
    }

    // --- Rolling interference band (slow drift) ---
    {
        float bandPos = frac(t * 0.06);
        float bandDist = min(abs(uv.y - bandPos), 1.0 - abs(uv.y - bandPos));
        float band = 1.0 / (1.0 + 30.0 * bandDist * bandDist);
        color += band * 0.06 * gParams.noiseIntensity;
    }

    // --- Color desaturation (VHS color degradation) ---
    float gray = dot(color, float3(0.299, 0.587, 0.114));
    color = lerp(color, float3(gray, gray, gray), 0.2);

    // --- Warm tint + contrast ---
    color = pow(max(color, 0.0), float3(1.08, 1.08, 1.12)) * 0.95;

    // --- Top / bottom edge degradation ---
    float edgeFade = smoothstep(0.0, 0.04, min(input.texcoord.y, 1.0 - input.texcoord.y));
    color *= edgeFade;

    return float4(saturate(color), 1.0);
}
