cbuffer GridConstants : register(b0)
{
    float4x4 invViewProj;
    float3 cameraPos;
    float gridHeight;
    float3 padding;
    float4x4 viewProj;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 nearPoint : TEXCOORD0;
    float3 farPoint : TEXCOORD1;
};

struct PSOutput
{
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

float4 ComputeGrid(float3 fragPos, float scale)
{
    float2 coord = fragPos.xz / scale;
    float2 derivative = fwidth(coord);
    float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
    float lineWidth = min(grid.x, grid.y);
    float alpha = 1.0 - min(lineWidth, 1.0);
    return float4(alpha, alpha, alpha, alpha);
}

float ComputeDepth(float3 pos, float4x4 viewProj)
{
    float4 clipPos = mul(float4(pos, 1.0), viewProj);
    return clipPos.z / clipPos.w;
}

PSOutput main(PSInput input)
{
    float t = (gridHeight - input.nearPoint.y) / (input.farPoint.y - input.nearPoint.y);
    
    PSOutput output;
    output.color = float4(0, 0, 0, 0);
    output.depth = 1.0;
    
    // レイが地面と交差しない場合は描画しない
    if (t < 0.0 || t > 1.0)
        discard;
    
    float3 fragPos = input.nearPoint + t * (input.farPoint - input.nearPoint);
    
    // 階層グリッド: 小グリッド(1.0) + 大グリッド(10.0)
    float4 gridSmall = ComputeGrid(fragPos, 1.0);
    float4 gridLarge = ComputeGrid(fragPos, 10.0);
    
    float4 grid = gridSmall * 0.3 + gridLarge * 0.7;
    
    // カメラからの距離でフェード
    float dist = length(fragPos - cameraPos);
    float fadeStart = 50.0;
    float fadeEnd = 100.0;
    float fade = 1.0 - smoothstep(fadeStart, fadeEnd, dist);
    
    grid.a *= fade;
    
    // アルファが低すぎる場合は描画しない
    if (grid.a < 0.01)
        discard;
    
    // グリッド色
    output.color = float4(0.5, 0.5, 0.5, grid.a * 0.5);
    
    // 深度計算
    output.depth = ComputeDepth(fragPos, viewProj);
    
    return output;
}
