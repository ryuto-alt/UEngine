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

Vector3 ConvertVector3(const aiVector3D& v) {
    return Vector3(v.x, v.y, v.z);
}

Quaternion ConvertQuaternion(const aiQuaternion& q) {
    return Quaternion(q.x, q.y, q.z, q.w);
}

Matrix4x4 ConvertMatrix(const aiMatrix4x4& m) {
    // AssimpのaiMatrix4x4: a1-a4が1行目, b1-b4が2行目, c1-c4が3行目, d1-d4が4行目
    // aiProcess_MakeLeftHandedで左手座標系に変換済み
    // しかしAssimpはcolumn-vector規約 (M*v)、DirectXはrow-vector規約 (v*M)
    // 規約変換のため転置が必要
    return Matrix4x4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

// ローカル変換行列を変換
Matrix4x4 ConvertLocalTransform(const aiMatrix4x4& m, const std::string& nodeName = "", float rootScale = 1.0f) {
    // 行列を分解
    aiVector3D scale, translate;
    aiQuaternion rotation;
    m.Decompose(scale, rotation, translate);

    // Mixamo対応: Hipsボーンまたはmixamorig:Hipsの異常なスケールを検出して修正
    bool isMixamoHips = (nodeName.find("Hips") != std::string::npos ||
                         nodeName.find("hips") != std::string::npos ||
                         nodeName.find("mixamorig") != std::string::npos);
    bool hasAbnormalScale = (scale.x < 0.1f || scale.y < 0.1f || scale.z < 0.1f);

    if (isMixamoHips && hasAbnormalScale) {
        scale = aiVector3D(1.0f, 1.0f, 1.0f);
    }

    // 座標変換: 回転のY,Z成分を反転、位置のX座標を反転
    Vector3 s(scale.x, scale.y, scale.z);
    Quaternion r(rotation.x, -rotation.y, -rotation.z, rotation.w);
    Vector3 t(-translate.x * rootScale, translate.y * rootScale, translate.z * rootScale);

    // S*R*T形式で構築
    return Matrix4x4::CreateScale(s) *
           Matrix4x4::CreateFromQuaternion(r) *
           Matrix4x4::CreateTranslation(t);
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
                                          std::unordered_map<std::string, int32>& boneMapping,
                                          float rootScale) {
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

        // アプローチ: InverseBindPose行列を分解して座標変換を適用
        const aiMatrix4x4& offsetMat = boneOffsets[name];

        // 1. InverseBindPoseの逆行列（BindPose）を取得
        aiMatrix4x4 bindPoseMatrix = offsetMat;
        bindPoseMatrix.Inverse();

        // 2. BindPoseを分解
        aiVector3D scale, translate;
        aiQuaternion rotation;
        bindPoseMatrix.Decompose(scale, rotation, translate);

        // Mixamo対応: 異常なスケール（100倍など）を検出して修正
        bool hasAbnormalScale = (scale.x > 10.0f || scale.y > 10.0f || scale.z > 10.0f ||
                                 scale.x < 0.1f || scale.y < 0.1f || scale.z < 0.1f);
        if (hasAbnormalScale) {
            scale = aiVector3D(1.0f, 1.0f, 1.0f);
        }

        // 3. 座標変換: 回転のY,Z成分を反転、位置のX座標を反転
        Vector3 s(scale.x, scale.y, scale.z);
        Quaternion r(rotation.x, -rotation.y, -rotation.z, rotation.w);
        Vector3 t(-translate.x, translate.y, translate.z);

        Matrix4x4 bindPoseConverted = Matrix4x4::CreateScale(s) *
                                      Matrix4x4::CreateFromQuaternion(r) *
                                      Matrix4x4::CreateTranslation(t);

        // 4. 変換後のBindPoseの逆行列がInverseBindPose
        Matrix4x4 offsetMatrix = bindPoseConverted.Inverse();



        Matrix4x4 localBindPose = Matrix4x4::Identity();
        if (boneNodes[name]) {
            localBindPose = ConvertLocalTransform(boneNodes[name]->mTransformation, name, rootScale);
        }

        skeleton->AddBone(name, parentIndex, offsetMatrix, localBindPose);
    }

    // GlobalInverseTransform = ルートノードの変換行列の逆行列
    // (ogldevチュートリアル参照)
    Matrix4x4 rootTransform = ConvertMatrix(scene->mRootNode->mTransformation);
    skeleton->SetGlobalInverseTransform(rootTransform.Inverse());

    return skeleton;
}

std::vector<std::shared_ptr<AnimationClip>> ExtractAnimations(const aiScene* scene,
                                                              const std::unordered_map<std::string, int32>& boneMapping) {
    std::vector<std::shared_ptr<AnimationClip>> clips;

    // ルートノードのスケールを取得（GLTFの場合、Armatureノードに0.01スケールが設定されている）
    float rootScale = 1.0f;

    // Armatureノードを再帰的に探す
    std::function<const aiNode*(const aiNode*)> findArmature = [&](const aiNode* node) -> const aiNode* {
        if (!node) return nullptr;

        std::string nodeName = node->mName.C_Str();
        if (nodeName.find("Armature") != std::string::npos || nodeName.find("armature") != std::string::npos) {
            return node;
        }

        for (uint32 i = 0; i < node->mNumChildren; ++i) {
            const aiNode* found = findArmature(node->mChildren[i]);
            if (found) return found;
        }
        return nullptr;
    };

    const aiNode* armatureNode = findArmature(scene->mRootNode);
    if (armatureNode) {
        aiVector3D scale, pos;
        aiQuaternion rot;
        armatureNode->mTransformation.Decompose(scale, rot, pos);

        float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
        if (avgScale < 1.0f && avgScale > 0.0001f) {
            rootScale = avgScale;
        }
    }

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
                const auto& p = channel->mPositionKeys[k].mValue;
                // 座標変換: X座標を反転、ルートスケールを適用
                key.value = Vector3(-p.x * rootScale, p.y * rootScale, p.z * rootScale);
                boneAnim.positionKeys.push_back(key);
            }

            for (uint32 k = 0; k < channel->mNumRotationKeys; ++k) {
                Keyframe<Quaternion> key;
                key.time = static_cast<float>(channel->mRotationKeys[k].mTime);
                const auto& q = channel->mRotationKeys[k].mValue;
                // 座標変換: Y,Z成分を反転
                key.value = Quaternion(q.x, -q.y, -q.z, q.w);
                boneAnim.rotationKeys.push_back(key);
            }

            for (uint32 k = 0; k < channel->mNumScalingKeys; ++k) {
                Keyframe<Vector3> key;
                key.time = static_cast<float>(channel->mScalingKeys[k].mTime);
                // スケールは座標系に依存しないので変換不要
                const auto& s = channel->mScalingKeys[k].mValue;
                key.value = Vector3(s.x, s.y, s.z);
                boneAnim.scaleKeys.push_back(key);
            }

            clip->AddBoneAnimation(boneAnim);
        }

        clips.push_back(clip);
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

    // Assimpはメッシュ頂点にノードtransformを自動適用しない

    for (uint32 i = 0; i < aiMesh->mNumVertices; ++i) {
        SkinnedVertex& vertex = vertices[i];

        // 座標変換: X座標を反転
        vertex.px = -aiMesh->mVertices[i].x;
        vertex.py = aiMesh->mVertices[i].y;
        vertex.pz = aiMesh->mVertices[i].z;

        if (aiMesh->HasNormals()) {
            // 法線もX成分を反転
            vertex.nx = -aiMesh->mNormals[i].x;
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

    for (uint32 i = 0; i < aiMesh->mNumFaces; ++i) {
        const aiFace& face = aiMesh->mFaces[i];
        if (face.mNumIndices == 3) {
            // : X軸反転により巻き順を反転（0, 2, 1の順序）
            indices.push_back(face.mIndices[0]);
            indices.push_back(face.mIndices[2]);
            indices.push_back(face.mIndices[1]);
        } else {
            for (uint32 j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(face.mIndices[j]);
            }
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
        // テクスチャ用のSRVインデックスを自動割り当て
        uint32 srvIndex = graphics->AllocateSRVIndex();
        mesh.LoadMaterial(materialData, graphics, commandList, baseDirectory, srvIndex);
    }

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

    // フラグを使用（aiProcess_MakeLeftHandedは使わない）
    // 座標変換は手動で行う
    unsigned int flags = aiProcess_Triangulate | aiProcess_FlipUVs |
                        aiProcess_LimitBoneWeights |
                        aiProcess_PopulateArmatureData;
    
    // aiProcess_GlobalScaleは使用しない（不完全な実装のため）
    // すべて手動でスケーリングする

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

    // ルートスケールを計算（GLTFの場合、Armatureノードに0.01スケールが設定されている）
    float rootScale = 1.0f;
    std::function<const aiNode*(const aiNode*)> findArmature = [&](const aiNode* node) -> const aiNode* {
        if (!node) return nullptr;
        std::string nodeName = node->mName.C_Str();
        if (nodeName.find("Armature") != std::string::npos || nodeName.find("armature") != std::string::npos) {
            return node;
        }
        for (uint32 i = 0; i < node->mNumChildren; ++i) {
            const aiNode* found = findArmature(node->mChildren[i]);
            if (found) return found;
        }
        return nullptr;
    };
    const aiNode* armatureNode = findArmature(scene->mRootNode);
    if (armatureNode) {
        aiVector3D scale, pos;
        aiQuaternion rot;
        armatureNode->mTransformation.Decompose(scale, rot, pos);
        float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
        if (avgScale < 1.0f && avgScale > 0.0001f) {
            rootScale = avgScale;
        }
    }

    std::unordered_map<std::string, int32> boneMapping;
    result.skeleton = ExtractSkeleton(scene, boneMapping, rootScale);

    result.animations = ExtractAnimations(scene, boneMapping);

    ProcessNode(scene->mRootNode, scene, graphics, commandList,
               baseDirectory, boneMapping, result.meshes);

    return result;
}

} // namespace UnoEngine
