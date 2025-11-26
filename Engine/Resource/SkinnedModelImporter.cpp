#include "SkinnedModelImporter.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Material.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <Windows.h>
#include <iostream>
#include <unordered_map>
#include <cfloat>
#include <algorithm>

namespace UnoEngine {

namespace {

void LogImportError(const std::string& message, const std::string& file) {
    std::string fullMessage = "[スキンモデル読み込みエラー]\n\n" + message + "\n\nファイル: " + file;
    std::cerr << fullMessage << std::endl;
    OutputDebugStringA((fullMessage + "\n").c_str());

    int wideSize = MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), -1, nullptr, 0);
    std::wstring wideMessage(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), -1, &wideMessage[0], wideSize);

    MessageBoxW(nullptr, wideMessage.c_str(), L"スキンモデル読み込みエラー", MB_OK | MB_ICONERROR);
}

Matrix4x4 ConvertMatrix(const aiMatrix4x4& m) {
    // Assimpは行優先（row-major）で格納、列ベクトル規約（v' = M * v）
    // DirectXも行優先だが行ベクトル規約（v' = v * M）
    // 直接コピーしてHLSL cbufferに書き込むと、
    // HLSLがcolumn-majorとして解釈するため自動的に転置される
    // これにより列ベクトル→行ベクトル変換が行われる
    return Matrix4x4(
        m.a1, m.a2, m.a3, m.a4,  // Row 0
        m.b1, m.b2, m.b3, m.b4,  // Row 1
        m.c1, m.c2, m.c3, m.c4,  // Row 2
        m.d1, m.d2, m.d3, m.d4   // Row 3
    );
}

Vector3 ConvertVector3(const aiVector3D& v) {
    return Vector3(v.x, v.y, v.z);
}

Quaternion ConvertQuaternion(const aiQuaternion& q) {
    return Quaternion(q.x, q.y, q.z, q.w);
}

MaterialData ConvertMaterial(const aiMaterial* aiMat, const std::string& baseDirectory) {
    MaterialData material;

    aiString name;
    if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
        material.name = name.C_Str();
    }

    aiColor3D color;
    if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
        material.ambient[0] = color.r;
        material.ambient[1] = color.g;
        material.ambient[2] = color.b;
    }

    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        material.diffuse[0] = color.r;
        material.diffuse[1] = color.g;
        material.diffuse[2] = color.b;
    }

    if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
        material.specular[0] = color.r;
        material.specular[1] = color.g;
        material.specular[2] = color.b;
    }

    aiString texPath;
    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
        namespace fs = std::filesystem;
        fs::path texturePath(texPath.C_Str());
        material.diffuseTexturePath = texturePath.filename().string();
    }

    return material;
}

void ProcessBoneHierarchy(const aiNode* node, const aiScene* scene,
                          Skeleton& skeleton, std::unordered_map<std::string, int32>& boneMapping,
                          int32 parentIndex = INVALID_BONE_INDEX) {
    std::string nodeName = node->mName.C_Str();

    auto it = boneMapping.find(nodeName);
    int32 currentIndex = INVALID_BONE_INDEX;

    if (it != boneMapping.end()) {
        currentIndex = it->second;
    }

    for (uint32 i = 0; i < node->mNumChildren; ++i) {
        ProcessBoneHierarchy(node->mChildren[i], scene, skeleton, boneMapping,
                            (currentIndex != INVALID_BONE_INDEX) ? currentIndex : parentIndex);
    }
}

std::shared_ptr<Skeleton> ExtractSkeleton(const aiScene* scene,
                                          std::unordered_map<std::string, int32>& boneMapping) {
    auto skeleton = std::make_shared<Skeleton>();

    std::unordered_map<std::string, aiMatrix4x4> boneOffsets;
    std::unordered_map<std::string, const aiNode*> boneNodes;

    for (uint32 m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        for (uint32 b = 0; b < mesh->mNumBones; ++b) {
            const aiBone* bone = mesh->mBones[b];
            std::string boneName = bone->mName.C_Str();

            if (boneMapping.find(boneName) == boneMapping.end()) {
                int32 index = static_cast<int32>(boneMapping.size());
                boneMapping[boneName] = index;
                boneOffsets[boneName] = bone->mOffsetMatrix;
            }
        }
    }

    std::function<const aiNode*(const aiNode*, const std::string&)> findNode =
        [&findNode](const aiNode* node, const std::string& name) -> const aiNode* {
        if (node->mName.C_Str() == name) {
            return node;
        }
        for (uint32 i = 0; i < node->mNumChildren; ++i) {
            const aiNode* found = findNode(node->mChildren[i], name);
            if (found) return found;
        }
        return nullptr;
    };

    for (const auto& [name, index] : boneMapping) {
        boneNodes[name] = findNode(scene->mRootNode, name);
    }

    auto findParentBone = [&](const std::string& boneName) -> int32 {
        const aiNode* node = boneNodes[boneName];
        if (!node || !node->mParent) return INVALID_BONE_INDEX;

        const aiNode* parent = node->mParent;
        while (parent) {
            std::string parentName = parent->mName.C_Str();
            auto it = boneMapping.find(parentName);
            if (it != boneMapping.end()) {
                return it->second;
            }
            parent = parent->mParent;
        }
        return INVALID_BONE_INDEX;
    };

    std::vector<std::pair<std::string, int32>> sortedBones(boneMapping.begin(), boneMapping.end());
    std::sort(sortedBones.begin(), sortedBones.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    for (const auto& [name, index] : sortedBones) {
        int32 parentIndex = findParentBone(name);

        // OffsetMatrixを取得
        Matrix4x4 offsetMatrix = ConvertMatrix(boneOffsets[name]);
        
        // aiProcess_GlobalScaleはinverseBindMatricesに影響しないため、
        // 手動でスケールを除去する必要がある
        // 各列ベクトルの長さを計算してスケールを検出
        float scaleX = 0.0f, scaleY = 0.0f, scaleZ = 0.0f;
        for (int row = 0; row < 3; ++row) {
            float x = offsetMatrix.GetElement(row, 0);
            float y = offsetMatrix.GetElement(row, 1);
            float z = offsetMatrix.GetElement(row, 2);
            scaleX += x * x;
            scaleY += y * y;
            scaleZ += z * z;
        }
        scaleX = std::sqrt(scaleX);
        scaleY = std::sqrt(scaleY);
        scaleZ = std::sqrt(scaleZ);
        
        // 平均スケールが100に近い場合（cm→m変換）、正規化
        float avgScale = (scaleX + scaleY + scaleZ) / 3.0f;
        if (avgScale > 10.0f) { // スケールが大きい場合のみ正規化
            // DirectXMath経由で行列要素を変更
            DirectX::XMFLOAT4X4 matFloat;
            DirectX::XMStoreFloat4x4(&matFloat, offsetMatrix.GetXMMatrix());
            
            // 回転成分の各列ベクトルを正規化
            if (scaleX > 0.0001f) {
                float invScale = 1.0f / scaleX;
                for (int row = 0; row < 3; ++row) {
                    matFloat.m[row][0] *= invScale;
                }
            }
            if (scaleY > 0.0001f) {
                float invScale = 1.0f / scaleY;
                for (int row = 0; row < 3; ++row) {
                    matFloat.m[row][1] *= invScale;
                }
            }
            if (scaleZ > 0.0001f) {
                float invScale = 1.0f / scaleZ;
                for (int row = 0; row < 3; ++row) {
                    matFloat.m[row][2] *= invScale;
                }
            }
            
            // 平行移動成分もスケーリング（cm→m変換）
            float posScale = 1.0f / avgScale;
            matFloat.m[3][0] *= posScale;
            matFloat.m[3][1] *= posScale;
            matFloat.m[3][2] *= posScale;
            // m[3][3]は常に1.0であるべき
            matFloat.m[3][3] = 1.0f;
            
            // 行列を再構築
            offsetMatrix = Matrix4x4(DirectX::XMLoadFloat4x4(&matFloat));
            
            // デバッグ: 最初のボーンの補正結果を出力
            if (index == 0) {
                char debugMsg[1024];
                sprintf_s(debugMsg, "Bone0 scale removed: avgScale=%.3f\n"
                         "  Row0: [%.3f, %.3f, %.3f, %.3f]\n"
                         "  Row1: [%.3f, %.3f, %.3f, %.3f]\n"
                         "  Row2: [%.3f, %.3f, %.3f, %.3f]\n"
                         "  Row3: [%.3f, %.3f, %.3f, %.3f]\n",
                         avgScale,
                         offsetMatrix.GetElement(0, 0), offsetMatrix.GetElement(0, 1),
                         offsetMatrix.GetElement(0, 2), offsetMatrix.GetElement(0, 3),
                         offsetMatrix.GetElement(1, 0), offsetMatrix.GetElement(1, 1),
                         offsetMatrix.GetElement(1, 2), offsetMatrix.GetElement(1, 3),
                         offsetMatrix.GetElement(2, 0), offsetMatrix.GetElement(2, 1),
                         offsetMatrix.GetElement(2, 2), offsetMatrix.GetElement(2, 3),
                         offsetMatrix.GetElement(3, 0), offsetMatrix.GetElement(3, 1),
                         offsetMatrix.GetElement(3, 2), offsetMatrix.GetElement(3, 3));
                OutputDebugStringA(debugMsg);
            }
        }

        Matrix4x4 localBindPose = Matrix4x4::Identity();
        if (boneNodes[name]) {
            localBindPose = ConvertMatrix(boneNodes[name]->mTransformation);
        }

        skeleton->AddBone(name, parentIndex, offsetMatrix, localBindPose);
    }

    // glTFの場合、ルートノードにスケール（例: 100）が含まれることがある
    // スキニングでは単位行列を使用し、OffsetMatrixで処理する
    // これにより、ボーン行列が正しく計算される
    skeleton->SetGlobalInverseTransform(Matrix4x4::Identity());

    char skelDebug[256];
    sprintf_s(skelDebug, "Skeleton created: %u bones\\n", skeleton->GetBoneCount());
    OutputDebugStringA(skelDebug);

    return skeleton;
}

std::vector<std::shared_ptr<AnimationClip>> ExtractAnimations(const aiScene* scene,
                                                              const std::unordered_map<std::string, int32>& boneMapping) {
    std::vector<std::shared_ptr<AnimationClip>> clips;

    for (uint32 a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* aiAnim = scene->mAnimations[a];

        auto clip = std::make_shared<AnimationClip>();
        clip->SetName(aiAnim->mName.C_Str());
        clip->SetDuration(static_cast<float>(aiAnim->mDuration));
        clip->SetTicksPerSecond(aiAnim->mTicksPerSecond > 0 ? static_cast<float>(aiAnim->mTicksPerSecond) : 25.0f);

        for (uint32 c = 0; c < aiAnim->mNumChannels; ++c) {
            const aiNodeAnim* channel = aiAnim->mChannels[c];
            std::string boneName = channel->mNodeName.C_Str();

            if (boneMapping.find(boneName) == boneMapping.end()) {
                continue;
            }

            BoneAnimation boneAnim;
            boneAnim.boneName = boneName;

            for (uint32 k = 0; k < channel->mNumPositionKeys; ++k) {
                Keyframe<Vector3> key;
                key.time = static_cast<float>(channel->mPositionKeys[k].mTime);
                key.value = ConvertVector3(channel->mPositionKeys[k].mValue);
                // AI_CONFIG_GLOBAL_SCALE_FACTOR_KEYにより自動スケーリング済み
                boneAnim.positionKeys.push_back(key);
            }

            for (uint32 k = 0; k < channel->mNumRotationKeys; ++k) {
                Keyframe<Quaternion> key;
                key.time = static_cast<float>(channel->mRotationKeys[k].mTime);
                key.value = ConvertQuaternion(channel->mRotationKeys[k].mValue);
                boneAnim.rotationKeys.push_back(key);
            }

            for (uint32 k = 0; k < channel->mNumScalingKeys; ++k) {
                Keyframe<Vector3> key;
                key.time = static_cast<float>(channel->mScalingKeys[k].mTime);
                key.value = ConvertVector3(channel->mScalingKeys[k].mValue);
                boneAnim.scaleKeys.push_back(key);
            }

            clip->AddBoneAnimation(boneAnim);
        }

        clips.push_back(clip);

        char debugMsg[256];
        sprintf_s(debugMsg, "Animation loaded: %s (%.2f sec, %u channels)\n",
                 clip->GetName().c_str(), clip->GetDuration() / clip->GetTicksPerSecond(),
                 aiAnim->mNumChannels);
        OutputDebugStringA(debugMsg);
    }

    return clips;
}

SkinnedMesh ProcessSkinnedMesh(const aiMesh* aiMesh, const aiScene* scene,
                               GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                               const std::string& baseDirectory,
                               const std::unordered_map<std::string, int32>& boneMapping) {
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32> indices;

    vertices.resize(aiMesh->mNumVertices);

    for (uint32 i = 0; i < aiMesh->mNumVertices; ++i) {
        SkinnedVertex& vertex = vertices[i];

        // AI_CONFIG_GLOBAL_SCALE_FACTOR_KEYにより自動スケーリング済み
        vertex.px = aiMesh->mVertices[i].x;
        vertex.py = aiMesh->mVertices[i].y;
        vertex.pz = aiMesh->mVertices[i].z;

        if (aiMesh->HasNormals()) {
            vertex.nx = aiMesh->mNormals[i].x;
            vertex.ny = aiMesh->mNormals[i].y;
            vertex.nz = aiMesh->mNormals[i].z;
        }

        if (aiMesh->HasTextureCoords(0)) {
            vertex.u = aiMesh->mTextureCoords[0][i].x;
            vertex.v = aiMesh->mTextureCoords[0][i].y;
        }
    }

    for (uint32 b = 0; b < aiMesh->mNumBones; ++b) {
        const aiBone* bone = aiMesh->mBones[b];
        std::string boneName = bone->mName.C_Str();

        auto it = boneMapping.find(boneName);
        if (it == boneMapping.end()) continue;

        int32 boneIndex = it->second;

        for (uint32 w = 0; w < bone->mNumWeights; ++w) {
            uint32 vertexId = bone->mWeights[w].mVertexId;
            float weight = bone->mWeights[w].mWeight;
            vertices[vertexId].AddBoneData(static_cast<uint32>(boneIndex), weight);
        }
    }

    for (auto& v : vertices) {
        v.NormalizeWeights();
    }

    // デバッグ: 最初の頂点のボーンデータ
    if (!vertices.empty()) {
        const auto& v0 = vertices[0];
        char boneDebug[512];
        sprintf_s(boneDebug, "Vertex[0] bones=[%u,%u,%u,%u], weights=[%.3f,%.3f,%.3f,%.3f]\n",
                 v0.boneIndices[0], v0.boneIndices[1], v0.boneIndices[2], v0.boneIndices[3],
                 v0.boneWeights[0], v0.boneWeights[1], v0.boneWeights[2], v0.boneWeights[3]);
        OutputDebugStringA(boneDebug);
    }

    // デバッグ: 頂点座標の範囲を出力
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
    for (const auto& v : vertices) {
        minX = (std::min)(minX, v.px);
        minY = (std::min)(minY, v.py);
        minZ = (std::min)(minZ, v.pz);
        maxX = (std::max)(maxX, v.px);
        maxY = (std::max)(maxY, v.py);
        maxZ = (std::max)(maxZ, v.pz);
    }
    char boundsMsg[256];
    sprintf_s(boundsMsg, "Mesh bounds: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
              minX, maxX, minY, maxY, minZ, maxZ);
    OutputDebugStringA(boundsMsg);

    for (uint32 i = 0; i < aiMesh->mNumFaces; ++i) {
        const aiFace& face = aiMesh->mFaces[i];
        for (uint32 j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    std::string meshName = aiMesh->mName.C_Str();
    if (meshName.empty()) {
        meshName = "skinned_mesh";
    }

    SkinnedMesh mesh;
    mesh.Create(graphics->GetDevice(), commandList, vertices, indices, meshName);

    if (aiMesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];
        MaterialData materialData = ConvertMaterial(aiMat, baseDirectory);
        mesh.LoadMaterial(materialData, graphics, commandList, baseDirectory, 0);
    }

    char debugMsg[256];
    sprintf_s(debugMsg, "SkinnedMesh loaded: %s - %zu vertices, %zu indices, %u bones\n",
             meshName.c_str(), vertices.size(), indices.size(), aiMesh->mNumBones);
    OutputDebugStringA(debugMsg);

    return mesh;
}

void ProcessNode(const aiNode* node, const aiScene* scene,
                 GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                 const std::string& baseDirectory,
                 const std::unordered_map<std::string, int32>& boneMapping,
                 std::vector<SkinnedMesh>& outMeshes) {
    for (uint32 i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        if (mesh->HasBones()) {
            outMeshes.push_back(ProcessSkinnedMesh(mesh, scene, graphics, commandList,
                                                   baseDirectory, boneMapping));
        }
    }

    for (uint32 i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene, graphics, commandList,
                   baseDirectory, boneMapping, outMeshes);
    }
}

} // anonymous namespace

SkinnedModelData SkinnedModelImporter::Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                            const std::string& filepath) {
    Assimp::Importer importer;

    unsigned int flags = aiProcess_Triangulate | aiProcess_FlipUVs |
                        aiProcess_CalcTangentSpace | aiProcess_GenNormals |
                        aiProcess_LimitBoneWeights |
                        aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder |
                        aiProcess_GlobalScale;

    // GLTFが100倍のスケール（cm単位）の場合、0.01倍してメートル単位に変換
    // AI_CONFIG_GLOBAL_SCALE_FACTOR_KEYはモデル全体に適用される
    importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 0.01f);

    const aiScene* scene = importer.ReadFile(filepath, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string errorMsg = "スキンモデルファイルを読み込めませんでした\n\n";
        errorMsg += "Assimpエラー: " + std::string(importer.GetErrorString());
        LogImportError(errorMsg, filepath);
        throw std::runtime_error("Failed to load skinned model: " + filepath);
    }

    namespace fs = std::filesystem;
    const fs::path modelPath(filepath);
    const std::string baseDirectory = modelPath.parent_path().string();

    SkinnedModelData result;

    std::unordered_map<std::string, int32> boneMapping;
    result.skeleton = ExtractSkeleton(scene, boneMapping);

    result.animations = ExtractAnimations(scene, boneMapping);

    ProcessNode(scene->mRootNode, scene, graphics, commandList,
               baseDirectory, boneMapping, result.meshes);

    const size_t lastSlash = filepath.find_last_of("/\\");
    const std::string name = (lastSlash != std::string::npos)
        ? filepath.substr(lastSlash + 1)
        : filepath;

    char debugMsg[512];
    sprintf_s(debugMsg, "SkinnedModel Loaded: %s - %zu meshes, %u bones, %zu animations\n",
             name.c_str(), result.meshes.size(), result.skeleton->GetBoneCount(),
             result.animations.size());
    OutputDebugStringA(debugMsg);

    return result;
}

} // namespace UnoEngine
