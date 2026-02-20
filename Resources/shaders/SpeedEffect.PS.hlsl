// Speed Effect: Red-black edge darkening aligned with 4:3 CRT content area

cbuffer SpeedParams : register(b0) {
    float sprintIntensity; // 0.0 ~ 1.0
    float time;
    float screenAspect;    // actual screen W/H  (e.g. 16/9 = 1.777)
    float targetAspect;    // 4:3 content aspect (= 1.333)
    float padding0, padding1, padding2, padding3;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input) {
    PSOutput output;
    float2 uv = input.texcoord;

    float4 scene = gTexture.Sample(gSampler, uv);

    if (sprintIntensity <= 0.001f) {
        output.color = scene;
        return output;
    }

    float3 color = scene.rgb;

    // --- Compute 4:3 content boundaries in UV space ---
    // contentWidth: fraction of screen width occupied by 4:3 content
    float contentWidth = targetAspect / screenAspect; // e.g. (4/3)/(16/9) = 0.75
    float xMin = (1.0f - contentWidth) * 0.5f;        // e.g. 0.125
    float xMax = xMin + contentWidth;                  // e.g. 0.875

    // Pixels in the pillarbox (outside content area): skip
    if (uv.x < xMin || uv.x > xMax) {
        output.color = scene;
        return output;
    }

    // --- Remap to content-local UV [0,1] ---
    float2 cUV;
    cUV.x = (uv.x - xMin) / contentWidth;
    cUV.y = uv.y;

    // --- Rectangular edge distance in content-local space ---
    float edgeX = abs(cUV.x - 0.5f) * 2.0f; // 0=center, 1=content left/right edge
    float edgeY = abs(cUV.y - 0.5f) * 2.0f; // 0=center, 1=content top/bottom edge

    // Weighted max: slightly emphasize left/right and top/bottom equally for 4:3
    float edgeDist = max(edgeX, edgeY);

    // Thin strip at the outer 22% of content area
    float band = smoothstep(0.78f, 1.02f, edgeDist);

    // Slow pulse for speed feel (not too fast)
    float pulse = 0.70f + sin(time * 3.5f) * 0.30f;

    float finalStrength = band * pulse * sprintIntensity;

    // Red-black gradient: deep red near inner edge, black at absolute edge
    float redFraction = 1.0f - smoothstep(0.78f, 1.02f, edgeDist);
    float3 edgeColor = float3(0.30f, 0.0f, 0.0f) * redFraction;

    color = lerp(color, edgeColor, finalStrength * 0.82f);

    output.color = float4(saturate(color), 1.0f);
    return output;
}
