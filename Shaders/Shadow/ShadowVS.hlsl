// Shadow Map Vertex Shader (static mesh, depth-only)

cbuffer ShadowTransform : register(b0) {
    matrix world;
    matrix lightViewProj;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

float4 main(VSInput input) : SV_POSITION {
    float4 worldPos = mul(float4(input.position, 1.0f), world);
    return mul(worldPos, lightViewProj);
}
