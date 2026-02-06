// VHS Tape Effect Pixel Shader
struct VSOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VHSParams {
    float time;
    float scanlineIntensity;
    float noiseIntensity;
    float trackingError;
    float chromaticAberration;
    float colorBleed;
    float sharpness;
    float tapeCrease;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<VHSParams> gParams : register(b0);

// High-frequency hash for per-frame random
float hash(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

// Static noise - changes every frame, no scrolling
float staticNoise(float2 uv, float seed) {
    return hash(uv * float2(1280.0, 720.0) + seed);
}

// Smooth noise for tracking distortion
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

float4 main(VSOutput input) : SV_TARGET {
    float2 uv = input.texcoord;
    float timeSeed = floor(gParams.time * 30.0); // Per-frame seed at 30fps

    // Resolution Reduction (pixelate first so everything gets affected)
    if (gParams.sharpness < 1.0) {
        float pixelSize = lerp(0.008, 0.001, gParams.sharpness);
        uv = floor(uv / pixelSize) * pixelSize;
    }

    // VHS Tracking Error (horizontal jitter per scanline)
    float trackingNoise = smoothNoise(float2(timeSeed, uv.y * 40.0));
    float trackingDisplacement = (trackingNoise - 0.5) * gParams.trackingError * 0.01;
    uv.x += trackingDisplacement;

    // Tape Crease (horizontal band of distortion that drifts vertically)
    float creasePos = frac(gParams.time * 0.08);
    float creaseDist = abs(uv.y - creasePos);
    if (creaseDist < 0.04) {
        float creaseIntensity = (0.04 - creaseDist) * 25.0;
        uv.x += sin(uv.y * 80.0) * creaseIntensity * gParams.tapeCrease * 0.015;
    }

    // Chromatic Aberration (RGB separation)
    float aberration = gParams.chromaticAberration * 0.002;
    float r = gTexture.Sample(gSampler, uv + float2(aberration, 0.0)).r;
    float g = gTexture.Sample(gSampler, uv).g;
    float b = gTexture.Sample(gSampler, uv - float2(aberration, 0.0)).b;
    float3 color = float3(r, g, b);

    // Color Bleed (horizontal smear)
    if (gParams.colorBleed > 0.0) {
        float3 bleed = float3(0, 0, 0);
        for (int i = -2; i <= 2; i++) {
            float offset = float(i) * 0.001 * gParams.colorBleed;
            bleed += gTexture.Sample(gSampler, uv + float2(offset, 0.0)).rgb;
        }
        color = lerp(color, bleed / 5.0, 0.3);
    }

    // Scan Lines
    float scanline = sin(input.texcoord.y * 720.0 * 3.14159) * 0.5 + 0.5;
    color *= 1.0 - (scanline * gParams.scanlineIntensity * 0.3);

    // VHS Static Noise (random per-pixel, per-frame flicker)
    float grain = staticNoise(input.texcoord, timeSeed);
    color += (grain - 0.5) * gParams.noiseIntensity * 0.15;

    // Occasional horizontal noise lines
    float lineNoise = hash(float2(timeSeed, floor(input.texcoord.y * 720.0)));
    if (lineNoise > 0.985) {
        color += gParams.noiseIntensity * 0.3;
    }

    // Color Desaturation (VHS color degradation)
    float gray = dot(color, float3(0.299, 0.587, 0.114));
    color = lerp(color, float3(gray, gray, gray), 0.25);

    // Contrast/Brightness (VHS characteristic warm tint)
    color = pow(max(color, 0.0), float3(1.1, 1.1, 1.1)) * 0.95;

    // Top/Bottom edge degradation
    float edgeFade = smoothstep(0.0, 0.04, min(input.texcoord.y, 1.0 - input.texcoord.y));
    color *= edgeFade;

    return float4(saturate(color), 1.0);
}
