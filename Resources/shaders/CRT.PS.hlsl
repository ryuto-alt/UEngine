// CRT Monitor Effect - 4:3 pillarbox with barrel distortion and rounded corners
struct VSOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct CRTParams {
    float cornerRadius;     // bezel rounding (0.04-0.10)
    float curvature;        // barrel distortion strength (0.03-0.15)
    float vignetteStrength; // phosphor glow falloff at edges
    float edgeSoftness;     // corner antialiasing width
    float screenAspect;     // actual output (16/9)
    float targetAspect;     // CRT content (4/3)
    float padding0;
    float padding1;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<CRTParams> gParams : register(b0);

// Barrel distortion (inverse mapping for CRT glass curvature)
float2 barrelDistort(float2 uv, float k) {
    float2 cc = uv - 0.5;
    float r2 = dot(cc, cc);
    cc *= 1.0 / (1.0 + k * r2);
    return cc + 0.5;
}

// Signed distance to rounded rectangle
float roundedRectSDF(float2 p, float2 halfSize, float radius) {
    float2 d = abs(p) - halfSize + radius;
    return length(max(d, 0.0)) - radius;
}

float4 main(VSOutput input) : SV_TARGET {
    float2 uv = input.texcoord;

    float pillarWidth = (1.0 - gParams.targetAspect / gParams.screenAspect) * 0.5;

    // Outside pillarbox -> pure black
    if (uv.x < pillarWidth || uv.x > 1.0 - pillarWidth) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    // Remap to content UV [0,1] within the 4:3 area
    float2 contentUV;
    contentUV.x = (uv.x - pillarWidth) / (1.0 - 2.0 * pillarWidth);
    contentUV.y = uv.y;

    // Rounded corner mask (CRT bezel shape)
    float2 centered = contentUV - 0.5;
    float sdfDist = roundedRectSDF(centered, float2(0.5, 0.5), gParams.cornerRadius);
    float mask = 1.0 - smoothstep(-gParams.edgeSoftness, gParams.edgeSoftness, sdfDist);

    if (mask <= 0.001) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    // Barrel distortion (CRT glass curvature)
    float2 distUV = barrelDistort(contentUV, gParams.curvature);

    // Out of bounds after distortion
    if (distUV.x < 0.0 || distUV.x > 1.0 || distUV.y < 0.0 || distUV.y > 1.0) {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    // Map distorted content UV back to input texture UV (center crop from 16:9)
    float2 sampleUV;
    sampleUV.x = distUV.x * (1.0 - 2.0 * pillarWidth) + pillarWidth;
    sampleUV.y = distUV.y;

    float3 color = gTexture.Sample(gSampler, sampleUV).rgb;

    // CRT phosphor vignette (edges darken naturally on real CRTs)
    float2 vigUV = contentUV * 2.0 - 1.0;
    float vignette = 1.0 - dot(vigUV, vigUV) * gParams.vignetteStrength;
    color *= saturate(vignette);

    // Apply rounded corner mask
    color *= mask;

    return float4(color, 1.0);
}
