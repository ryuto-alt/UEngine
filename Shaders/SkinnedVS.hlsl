// Skinned Vertex Shader - GPU Skinning

#define MAX_BONES 256

cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
};

cbuffer BoneMatrices : register(b3) {
    matrix bones[MAX_BONES];
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput main(VSInput input) {
    VSOutput output;

    // デバッグ: スキニングをバイパス、生の頂点位置を使用
    float4 localPos = float4(input.position, 1.0f);

    // ワールド空間位置
    float4 worldPos = mul(localPos, world);
    output.worldPos = worldPos.xyz;

    // クリップ空間位置
    output.position = mul(localPos, mvp);

    // ワールド空間法線
    output.normal = normalize(mul(input.normal, (float3x3)world));

    output.uv = input.uv;

    return output;
}
