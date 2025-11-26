#pragma once

#include "../Core/Types.h"

namespace UnoEngine {

constexpr uint32 MAX_BONE_INFLUENCE = 4;

struct SkinnedVertex {
    float px, py, pz;           // Position
    float nx, ny, nz;           // Normal
    float u, v;                 // TexCoord
    uint32 boneIndices[MAX_BONE_INFLUENCE];  // ボーンインデックス
    float boneWeights[MAX_BONE_INFLUENCE];   // ボーンウェイト

    SkinnedVertex() {
        px = py = pz = 0.0f;
        nx = ny = nz = 0.0f;
        u = v = 0.0f;
        for (uint32 i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            boneIndices[i] = 0;
            boneWeights[i] = 0.0f;
        }
    }

    void AddBoneData(uint32 boneIndex, float weight) {
        for (uint32 i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            if (boneWeights[i] < 0.0001f) {
                boneIndices[i] = boneIndex;
                boneWeights[i] = weight;
                return;
            }
        }
    }

    void NormalizeWeights() {
        float totalWeight = 0.0f;
        for (uint32 i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            totalWeight += boneWeights[i];
        }
        if (totalWeight > 0.0001f) {
            for (uint32 i = 0; i < MAX_BONE_INFLUENCE; ++i) {
                boneWeights[i] /= totalWeight;
            }
        } else {
            boneWeights[0] = 1.0f;
        }
    }
};

} // namespace UnoEngine
