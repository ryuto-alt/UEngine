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

    // GPUスキニング: ボーン行列による頂点変換
    // DirectXMathとHLSL両方が行優先なので、行ベクトル形式で乗算
    float4 skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    // 各ボーンの影響を加算
    [unroll]
    for (int i = 0; i < 4; i++) {
        uint boneIdx = input.boneIndices[i];
        float weight = input.boneWeights[i];

        if (weight > 0.0f) {
            matrix boneMat = bones[boneIdx];
            // 行ベクトル形式: mul(vec, matrix)
            skinnedPos += weight * mul(float4(input.position, 1.0f), boneMat);
            skinnedNormal += weight * mul(input.normal, (float3x3)boneMat);
        }
    }

    // ウェイトの合計が0の場合はバインドポーズを使用
    float totalWeight = input.boneWeights.x + input.boneWeights.y + input.boneWeights.z + input.boneWeights.w;
    if (totalWeight < 0.001f) {
        skinnedPos = float4(input.position, 1.0f);
        skinnedNormal = input.normal;
    }

    // ワールド空間位置
    float4 worldPos = mul(skinnedPos, world);
    output.worldPos = worldPos.xyz;

    // クリップ空間位置
    output.position = mul(skinnedPos, mvp);

    // ワールド空間法線
    output.normal = normalize(mul(skinnedNormal, (float3x3)world));

    output.uv = input.uv;

    return output;
}
