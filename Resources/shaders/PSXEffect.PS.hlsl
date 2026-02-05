// PS1/PSX retro post-process
// Pixelation + 15-bit color reduction + hardware-accurate 4x4 ordered dithering

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

cbuffer PSXParams : register(b0)
{
    float32_t screenWidth;
    float32_t screenHeight;
    float32_t targetWidth;
    float32_t targetHeight;
    int32_t   colorDepth;
    int32_t   enableDithering;
    float32_t padding0;
    float32_t padding1;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0); // Must be POINT filtering

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// Authentic PS1 dither matrix (psx-spx hardware documentation)
static const int PSX_DITHER_TABLE[4][4] =
{
    { -4, +0, -3, +1 },
    { +2, -2, +3, -1 },
    { -3, +1, -4, +0 },
    { +3, -1, +2, -2 }
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // --- Pixelation: quantize UV to target resolution ---
    float32_t2 pixelatedUV = floor(input.texcoord * float32_t2(targetWidth, targetHeight))
                           / float32_t2(targetWidth, targetHeight);
    float32_t3 color = gTexture.Sample(gSampler, pixelatedUV).rgb;

    // Dither position in the low-res grid
    int2 ditherPos = int2(floor(input.texcoord * float32_t2(targetWidth, targetHeight)));

    if (enableDithering)
    {
        // Apply PS1 hardware dither offset before color truncation
        int ditherValue = PSX_DITHER_TABLE[ditherPos.y % 4][ditherPos.x % 4];
        color = color * 255.0 + float32_t(ditherValue);
        color = clamp(color, 0.0, 255.0);
    }
    else
    {
        color *= 255.0;
    }

    // --- Color depth truncation (8-bit -> N-bit) ---
    int32_t shift = 8 - colorDepth;
    int32_t maxVal = (1U << colorDepth) - 1;

    int3 quantized = int3(color) >> shift;
    quantized = clamp(quantized, 0, maxVal);

    output.color = float32_t4(float32_t3(quantized) / float32_t(maxVal), 1.0);
    return output;
}
