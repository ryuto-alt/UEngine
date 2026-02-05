// Title screen noise/glitch post-process
// Film grain + scanlines + block glitch + chromatic aberration + vignette

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

cbuffer TitleNoiseParams : register(b0)
{
    float32_t time;
    float32_t grainIntensity;
    float32_t scanlineIntensity;
    float32_t scanlineCount;
    float32_t glitchIntensity;
    float32_t glitchFrequency;
    float32_t chromaticStrength;
    float32_t vignetteIntensity;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

float32_t Hash(float32_t2 p)
{
    return frac(sin(dot(p, float32_t2(12.9898, 78.233))) * 43758.5453);
}

// Luminance-shaped film grain: stronger in midtones
float32_t FilmGrain(float32_t2 uv, float32_t luminance)
{
    float32_t2 seed = uv + float32_t2(time * 7.13, time * 5.71);
    float32_t noise = Hash(seed) * 2.0 - 1.0;
    float32_t shape = 1.0 - (2.0 * luminance - 1.0) * (2.0 * luminance - 1.0);
    return noise * grainIntensity * shape;
}

float32_t Scanlines(float32_t2 uv)
{
    float32_t scanVal = sin((uv.y + time * 0.05) * scanlineCount * 3.14159265);
    scanVal = scanVal * 0.5 + 0.5;
    scanVal = pow(scanVal, 1.5);
    return lerp(1.0, scanVal, scanlineIntensity);
}

// VHS-style horizontal block displacement
float32_t GlitchOffset(float32_t2 uv)
{
    float32_t blockSize = 0.03;
    float32_t quantizedTime = floor(time * 10.0) * 0.1;
    float32_t trigger = step(1.0 - glitchFrequency, Hash(float32_t2(quantizedTime, 0.0)));

    float32_t blockY = floor(uv.y / blockSize);
    float32_t offset = (Hash(float32_t2(blockY, quantizedTime)) * 2.0 - 1.0) * 0.05;
    float32_t blockMask = step(0.6, Hash(float32_t2(blockY + 100.0, quantizedTime)));

    return offset * trigger * blockMask * glitchIntensity;
}

float32_t3 ChromaticAberration(float32_t2 uv)
{
    float32_t2 direction = uv - float32_t2(0.5, 0.5);
    float32_t dist = length(direction);
    float32_t aberration = chromaticStrength * dist;

    float32_t r = gTexture.Sample(gSampler, uv + direction * aberration).r;
    float32_t g = gTexture.Sample(gSampler, uv).g;
    float32_t b = gTexture.Sample(gSampler, uv - direction * aberration).b;

    return float32_t3(r, g, b);
}

float32_t Vignette(float32_t2 uv)
{
    float32_t dist = distance(uv, float32_t2(0.5, 0.5));
    return 1.0 - smoothstep(0.3, 0.9, dist) * vignetteIntensity;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;

    // Block glitch displacement
    uv.x += GlitchOffset(uv);

    // Chromatic aberration (samples scene texture)
    float32_t3 color = ChromaticAberration(uv);

    // Scanlines
    color *= Scanlines(input.texcoord);

    // Film grain
    float32_t luma = dot(color, float32_t3(0.2126, 0.7152, 0.0722));
    color += FilmGrain(input.texcoord, luma);

    // Vignette
    color *= Vignette(input.texcoord);

    output.color = float32_t4(saturate(color), 1.0);
    return output;
}
