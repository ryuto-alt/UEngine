cbuffer GridConstants : register(b0)
{
    float4x4 invViewProj;
    float3 cameraPos;
    float gridHeight;
    float3 padding;
    float4x4 viewProj;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 nearPoint : TEXCOORD0;
    float3 farPoint : TEXCOORD1;
};

float3 UnprojectPoint(float x, float y, float z)
{
    float4 unprojected = mul(float4(x, y, z, 1.0), invViewProj);
    return unprojected.xyz / unprojected.w;
}

VSOutput main(uint vertexID : SV_VertexID)
{
    // フルスクリーンクワッド生成
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    float2 pos = uv * 2.0 - 1.0;
    
    VSOutput output;
    output.position = float4(pos, 0.0, 1.0);
    output.nearPoint = UnprojectPoint(pos.x, pos.y, 0.0);
    output.farPoint = UnprojectPoint(pos.x, pos.y, 1.0);
    
    return output;
}
