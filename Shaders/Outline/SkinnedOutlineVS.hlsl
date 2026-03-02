#define MAX_BONES 256

cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
};

struct BoneMatrixPair {
    matrix skeletonSpaceMatrix;
    matrix skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<BoneMatrixPair> gMatrixPalette : register(t0);

cbuffer OutlineParams : register(b1) {
    float outlineWidth;
    float3 outlineColor;
};

struct VSInput {
    float3 position     : POSITION;
    float3 normal       : NORMAL;
    float2 uv           : TEXCOORD;
    uint4  boneIndices  : BLENDINDICES;
    float4 boneWeights  : BLENDWEIGHT;
};

float4 main(VSInput input) : SV_POSITION {
    float4 skinnedPos =
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.x].skeletonSpaceMatrix) * input.boneWeights.x +
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.y].skeletonSpaceMatrix) * input.boneWeights.y +
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.z].skeletonSpaceMatrix) * input.boneWeights.z +
        mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.w].skeletonSpaceMatrix) * input.boneWeights.w;
    skinnedPos.w = 1.0f;

    float3 skinnedNormal =
        mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.x].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.x +
        mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.y].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.y +
        mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.z].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.z +
        mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.w].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.w;
    skinnedNormal = normalize(skinnedNormal);

    skinnedPos.xyz += skinnedNormal * outlineWidth;

    return mul(skinnedPos, mvp);
}
