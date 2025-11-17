// PBR Vertex Shader

cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input) {
    VSOutput output;

    // ワールド空間位置
    float4 worldPos = mul(float4(input.position, 1.0f), world);
    output.worldPos = worldPos.xyz;

    // クリップ空間位置
    output.position = mul(float4(input.position, 1.0f), mvp);

    // ワールド空間法線（正規化）
    output.normal = normalize(mul(input.normal, (float3x3)world));

    output.uv = input.uv;

    return output;
}
