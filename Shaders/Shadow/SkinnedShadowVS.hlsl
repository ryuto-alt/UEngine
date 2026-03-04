// Shadow Map Vertex Shader (skinned mesh, depth-only)

#define MAX_BONES 256

cbuffer ShadowTransform : register(b0) {
    matrix world;
    matrix lightViewProj;
};

struct BoneMatrixPair {
    matrix skeletonSpaceMatrix;
    matrix skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<BoneMatrixPair> gMatrixPalette : register(t0);

struct VSInput {
    float3 position    : POSITION;
    float3 normal      : NORMAL;
    float2 uv          : TEXCOORD;
    uint4  boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

float4 main(VSInput input) : SV_POSITION {
    float4 skinnedPos =
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.x].skeletonSpaceMatrix) * input.boneWeights.x +
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.y].skeletonSpaceMatrix) * input.boneWeights.y +
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.z].skeletonSpaceMatrix) * input.boneWeights.z +
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.w].skeletonSpaceMatrix) * input.boneWeights.w;
    skinnedPos.w = 1.0f;

    float4 worldPos = mul(skinnedPos, world);
    return mul(worldPos, lightViewProj);
}
