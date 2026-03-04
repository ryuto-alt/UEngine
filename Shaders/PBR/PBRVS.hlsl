// PBR Vertex Shader

cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
    matrix lightViewProj; // for shadow mapping
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct VSOutput {
    float4 position  : SV_POSITION;
    float3 worldPos  : POSITION0;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    float4 shadowPos : TEXCOORD1;
};

VSOutput main(VSInput input) {
    VSOutput output;

    float4 worldPos    = mul(float4(input.position, 1.0f), world);
    output.worldPos    = worldPos.xyz;
    output.position    = mul(float4(input.position, 1.0f), mvp);
    output.normal      = normalize(mul(input.normal, (float3x3)world));
    output.uv          = input.uv;
    output.shadowPos   = mul(worldPos, lightViewProj);

    return output;
}
