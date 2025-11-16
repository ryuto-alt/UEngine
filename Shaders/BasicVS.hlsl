// Basic Vertex Shader

cbuffer Transform : register(b0) {
    matrix mvp;  // Model-View-Projection matrix
};

struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.color = input.color;
    return output;
}
