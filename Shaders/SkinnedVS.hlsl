// Skinned Vertex Shader - GPU Skinning

#define MAX_BONES 256

cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
};

// ボーン行列ペア構造体
struct BoneMatrixPair {
    matrix skeletonSpaceMatrix;
    matrix skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<BoneMatrixPair> gMatrixPalette : register(t0);

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

// スキニング計算関数
struct SkinnedVertex {
    float4 position;
    float3 normal;
};

SkinnedVertex Skinning(VSInput input) {
    SkinnedVertex skinned;

    // スキニング計算：各ボーンの影響を加重平均
    // prohと同じ: mul(vector, matrix)形式
    skinned.position = mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.x].skeletonSpaceMatrix) * input.boneWeights.x;
    skinned.position += mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.y].skeletonSpaceMatrix) * input.boneWeights.y;
    skinned.position += mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.z].skeletonSpaceMatrix) * input.boneWeights.z;
    skinned.position += mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.w].skeletonSpaceMatrix) * input.boneWeights.w;
    skinned.position.w = 1.0f;

    // 法線のスキニング（InverseTranspose行列を使用）
    skinned.normal = mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.x].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.x;
    skinned.normal += mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.y].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.y;
    skinned.normal += mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.z].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.z;
    skinned.normal += mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.w].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.w;
    skinned.normal = normalize(skinned.normal);

    return skinned;
}

VSOutput main(VSInput input) {
    VSOutput output;

    // スキニング処理
    SkinnedVertex skinned = Skinning(input);

    // ワールド空間位置
    float4 worldPos = mul(skinned.position, world);
    output.worldPos = worldPos.xyz;

    // クリップ空間位置
    output.position = mul(skinned.position, mvp);

    // ワールド空間法線
    output.normal = normalize(mul(skinned.normal, (float3x3)world));

    output.uv = input.uv;

    return output;
}