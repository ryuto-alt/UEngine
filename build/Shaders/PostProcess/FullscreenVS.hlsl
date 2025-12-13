// Fullscreen Triangle Vertex Shader
// Generates a fullscreen triangle without vertex buffer

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput output;

    // Generate fullscreen triangle vertices
    // vertexID: 0 -> (-1, -1), 1 -> (3, -1), 2 -> (-1, 3)
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texCoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return output;
}
