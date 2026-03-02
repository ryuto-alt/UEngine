cbuffer GridConstants : register(b0)
{
    float4x4 invViewProj;
    float3 cameraPos;
    float gridHeight;
    float3 padding;
    float padding2;
    float4x4 viewProj;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 nearPoint : NEAR_POINT;
    float3 farPoint : FAR_POINT;
};

struct PSOutput
{
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

float4 Grid(float3 fragPos3D, float scale)
{
    float2 coord = fragPos3D.xz * scale;
    float2 derivative = fwidth(coord);
    float2 grid = abs(frac(coord - 0.5f) - 0.5f) / derivative;
    float lineVal = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1.0f);
    float minimumx = min(derivative.x, 1.0f);

    float4 color = float4(0.3f, 0.3f, 0.3f, 1.0f - min(lineVal, 1.0f));

    // Z axis (blue)
    if (fragPos3D.x > -0.1f * minimumx && fragPos3D.x < 0.1f * minimumx)
        color = float4(0.0f, 0.0f, 1.0f, color.a);

    // X axis (red)
    if (fragPos3D.z > -0.1f * minimumz && fragPos3D.z < 0.1f * minimumz)
        color = float4(1.0f, 0.0f, 0.0f, color.a);

    return color;
}

float ComputeDepth(float3 pos)
{
    float4 clipPos = mul(float4(pos, 1.0f), viewProj);
    return clipPos.z / clipPos.w;
}

PSOutput main(PSInput input)
{
    PSOutput output;

    float t = -(input.nearPoint.y - gridHeight) / (input.farPoint.y - input.nearPoint.y);

    if (t < 0.0f)
        discard;

    float3 fragPos3D = input.nearPoint + t * (input.farPoint - input.nearPoint);

    output.depth = ComputeDepth(fragPos3D);

    // Distance-based fading
    float dist = length(fragPos3D.xz - cameraPos.xz);
    float fading = saturate(1.0f - dist / 100.0f);

    float4 gridColor = Grid(fragPos3D, 1.0f) + Grid(fragPos3D, 0.1f);
    gridColor.a *= fading;

    if (gridColor.a < 0.01f)
        discard;

    output.color = gridColor;
    return output;
}
