cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
};

cbuffer OutlineParams : register(b1) {
    float outlineWidth;
    float3 outlineColor;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

float4 main(VSInput input) : SV_POSITION {
    float3 expandedPos = input.position + normalize(input.normal) * outlineWidth;
    return mul(float4(expandedPos, 1.0f), mvp);
}
