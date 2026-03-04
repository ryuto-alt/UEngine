// Skinned Vertex Shader - GPU Skinning

#define MAX_BONES 256

cbuffer Transform : register(b0) {
    matrix world;
    matrix view;
    matrix projection;
    matrix mvp;
    matrix lightViewProj; // for shadow mapping
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

struct VSOutput {
    float4 position  : SV_POSITION;
    float3 worldPos  : POSITION0;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD0;
    float4 shadowPos : TEXCOORD1;
};

struct SkinnedVertex {
    float4 position;
    float3 normal;
};

SkinnedVertex Skinning(VSInput input) {
    SkinnedVertex skinned;

    skinned.position  = mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.x].skeletonSpaceMatrix) * input.boneWeights.x;
    skinned.position += mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.y].skeletonSpaceMatrix) * input.boneWeights.y;
    skinned.position += mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.z].skeletonSpaceMatrix) * input.boneWeights.z;
    skinned.position += mul(float4(input.position, 1.0f), gMatrixPalette[input.boneIndices.w].skeletonSpaceMatrix) * input.boneWeights.w;
    skinned.position.w = 1.0f;

    skinned.normal  = mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.x].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.x;
    skinned.normal += mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.y].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.y;
    skinned.normal += mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.z].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.z;
    skinned.normal += mul(input.normal, (float3x3)gMatrixPalette[input.boneIndices.w].skeletonSpaceInverseTransposeMatrix) * input.boneWeights.w;
    skinned.normal  = normalize(skinned.normal);

    return skinned;
}

VSOutput main(VSInput input) {
    VSOutput output;

    SkinnedVertex skinned = Skinning(input);

    float4 worldPos    = mul(skinned.position, world);
    output.worldPos    = worldPos.xyz;
    output.position    = mul(skinned.position, mvp);
    output.normal      = normalize(mul(skinned.normal, (float3x3)world));
    output.uv          = input.uv;
    output.shadowPos   = mul(worldPos, lightViewProj);

    return output;
}
