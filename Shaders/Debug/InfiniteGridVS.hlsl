cbuffer GridConstants : register(b0)
{
    float4x4 invViewProj;
    float3 cameraPos;
    float gridHeight;
    float3 padding;
    float padding2;
    float4x4 viewProj;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 nearPoint : NEAR_POINT;
    float3 farPoint : FAR_POINT;
};

float3 UnprojectPoint(float x, float y, float z)
{
    float4 clipPoint = float4(x, y, z, 1.0f);
    float4 worldPoint = mul(clipPoint, invViewProj);
    return worldPoint.xyz / worldPoint.w;
}

VSOutput main(uint vertexID : SV_VertexID)
{
    // Quad (4 vertices, triangle strip)
    float2 positions[4] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  1.0f),
        float2( 1.0f, -1.0f),
        float2( 1.0f,  1.0f)
    };

    float2 pos = positions[vertexID];

    VSOutput output;
    output.nearPoint = UnprojectPoint(pos.x, pos.y, 0.0f);
    output.farPoint = UnprojectPoint(pos.x, pos.y, 1.0f);
    output.position = float4(pos, 0.0f, 1.0f);
    return output;
}
