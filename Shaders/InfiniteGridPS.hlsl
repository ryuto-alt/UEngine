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
    
    // 軸ライン（X軸=赤、Z軸=青）- 控えめな色
    float axisWidth = 0.02;  // 軸ラインの太さ（細め）
    float3 axisColor = float3(0.3, 0.3, 0.3);  // デフォルト（暗い灰色）
    float axisAlpha = 0.0;
    
    // X軸（Z=0付近）- 控えめな赤
    if (abs(fragPos.z) < axisWidth) {
        axisColor = float3(0.5, 0.2, 0.2);  // 暗めの赤
        axisAlpha = (1.0 - abs(fragPos.z) / axisWidth) * 0.6;
    }
    // Z軸（X=0付近）- 控えめな青
    if (abs(fragPos.x) < axisWidth) {
        axisColor = float3(0.2, 0.3, 0.5);  // 暗めの青
        axisAlpha = max(axisAlpha, (1.0 - abs(fragPos.x) / axisWidth) * 0.6);
    }
    
    // グリッド色（非常に薄い灰色）
    float3 gridColor = float3(0.25, 0.25, 0.25);
    float gridAlpha = grid.a * 0.15;  // かなり薄く
    
    // 軸ラインとグリッドを合成
    float3 finalColor = lerp(gridColor, axisColor, axisAlpha);
    float finalAlpha = max(gridAlpha, axisAlpha * fade);
    
    output.color = float4(finalColor, finalAlpha);
    
    // 深度計算
    output.depth = ComputeDepth(fragPos, viewProj);
    
    return output;
}
