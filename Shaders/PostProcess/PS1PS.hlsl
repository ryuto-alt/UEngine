// PS1 Retro Post Process Pixel Shader
// Emulates PlayStation 1 graphics: color depth reduction, dithering, resolution downscaling

cbuffer PS1CB : register(b0) {
    float colorDepth;      // Bits per channel (1-8, PS1 typically 5 = 15-bit color)
    float resolutionScale; // Resolution reduction factor (4 = 320x240 from 1280x960)
    float ditherEnabled;   // 1.0 = enabled, 0.0 = disabled
    float ditherStrength;  // Dithering intensity multiplier
    float screenWidth;
    float screenHeight;
    float2 padding;
};

Texture2D inputTexture : register(t0);
SamplerState pointSampler : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// PS1-authentic 4x4 dither matrix from PSXACT emulator
// Uses both positive and negative values to preserve color range at extremes
static const float4x4 ditherMatrix = float4x4(
    -4.0,  0.0, -3.0,  1.0,
     2.0, -2.0,  3.0, -1.0,
    -3.0,  1.0, -4.0,  0.0,
     3.0, -1.0,  2.0, -2.0
);

float3 ApplyDithering(float3 color, int2 pixelPos) {
    // Get dither value from matrix (tiled across screen)
    float dither = ditherMatrix[pixelPos.x % 4][pixelPos.y % 4];
    
    // Scale dither based on color depth (lower depth = stronger visible effect)
    float ditherScale = ditherStrength / (pow(2.0, colorDepth) - 1.0);
    
    return color + dither * ditherScale * 0.1;
}

float3 QuantizeColor(float3 color) {
    // Calculate number of color levels per channel
    float levels = pow(2.0, colorDepth);
    
    // Quantize to discrete levels
    color = floor(color * levels) / (levels - 1.0);
    
    return saturate(color);
}

float2 PixelateUV(float2 uv) {
    // Calculate low-res pixel grid
    float2 lowResSize = float2(screenWidth, screenHeight) / resolutionScale;
    
    // Snap UV to low-res pixel centers
    float2 pixel = floor(uv * lowResSize);
    return (pixel + 0.5) / lowResSize;
}

float4 main(PSInput input) : SV_TARGET {
    // Step 1: Resolution reduction (pixelation)
    float2 pixelatedUV = PixelateUV(input.texCoord);
    
    // Sample with point filtering for sharp pixels
    float4 color = inputTexture.Sample(pointSampler, pixelatedUV);
    
    // Calculate actual pixel position for dithering
    int2 pixelPos = int2(input.position.xy);
    
    // Step 2: Apply PS1-style dithering (before quantization)
    if (ditherEnabled > 0.5) {
        color.rgb = ApplyDithering(color.rgb, pixelPos);
    }
    
    // Step 3: Color depth reduction (quantization)
    color.rgb = QuantizeColor(color.rgb);
    
    return float4(color.rgb, color.a);
}
