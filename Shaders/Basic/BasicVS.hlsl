// Basic Vertex Shader

cbuffer Transform : register(b0) {
    matrix mvp;  // Model-View-Projection matrix
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}
