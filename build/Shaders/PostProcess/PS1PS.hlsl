cbuffer PS1CB : register(b0) {
    float colorDepth;      
    float resolutionScale; 
    float ditherEnabled;   
    float ditherStrength;  
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



static const float4x4 ditherMatrix = float4x4(
    -4.0,  0.0, -3.0,  1.0,
     2.0, -2.0,  3.0, -1.0,
    -3.0,  1.0, -4.0,  0.0,
     3.0, -1.0,  2.0, -2.0
);

float3 ApplyDithering(float3 color, int2 pixelPos) {
 
    float dither = ditherMatrix[pixelPos.x % 4][pixelPos.y % 4];
    
    
    float ditherScale = ditherStrength / (pow(2.0, colorDepth) - 1.0);
    
    return color + dither * ditherScale * 0.1;
}

float3 QuantizeColor(float3 color) {
    
    float levels = pow(2.0, colorDepth);
    
    
    color = floor(color * levels) / (levels - 1.0);
    
    return saturate(color);
}

float2 PixelateUV(float2 uv) {
    
    float2 lowResSize = float2(screenWidth, screenHeight) / resolutionScale;
    
   
    float2 pixel = floor(uv * lowResSize);
    return (pixel + 0.5) / lowResSize;
}

float4 main(PSInput input) : SV_TARGET {
    
    float2 pixelatedUV = PixelateUV(input.texCoord);
    
    
    float4 color = inputTexture.Sample(pointSampler, pixelatedUV);
    
    
    int2 pixelPos = int2(input.position.xy);
    
    
    if (ditherEnabled > 0.5) {
        color.rgb = ApplyDithering(color.rgb, pixelPos);
    }
    
   
    color.rgb = QuantizeColor(color.rgb);
    
    return float4(color.rgb, color.a);
}
