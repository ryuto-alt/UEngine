#include "pch.h"
#include "StaticModelImporter.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Material.h"
#include "../Core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <filesystem>
#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <cfloat>

namespace UnoEngine {

namespace {

void LogImportError(const std::string& message, const std::string& file) {
    std::string fullMessage = "[静的モデル読み込みエラー]\n\n" + message + "\n\nファイル: " + file;
    std::cerr << fullMessage << std::endl;
    OutputDebugStringA((fullMessage + "\n").c_str());

    int wideSize = MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), -1, nullptr, 0);
    std::wstring wideMessage(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), -1, &wideMessage[0], wideSize);

    MessageBoxW(nullptr, wideMessage.c_str(), L"静的モデル読み込みエラー", MB_OK | MB_ICONERROR);
}

std::string UrlDecode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            unsigned int value = 0;
            if (sscanf_s(encoded.substr(i + 1, 2).c_str(), "%x", &value) == 1) {
                decoded += static_cast<char>(value);
                i += 2;
                continue;
            }
        }
        decoded += encoded[i];
    }
    return decoded;
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

    float opacity = 1.0f;
    if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
        material.opacity = opacity;
    }
    if (material.opacity < 1.0f) {
        material.useAlphaClip = true;
    }

    // glTF alphaMode検出 (MASK or BLEND)
    aiString alphaMode;
    if (aiMat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
        std::string mode = alphaMode.C_Str();
        if (mode == "MASK") {
            material.useAlphaClip = true;
            float cutoff = 0.5f;
            if (aiMat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, cutoff) == AI_SUCCESS) {
                material.alphaClipThreshold = cutoff;
            }
        } else if (mode == "BLEND") {
            material.useAlphaBlend = true;    // BLENDモード: アルファブレンドで半透明描画
            material.doubleSided = true;      // BLENDは通常doubleSided
        }
    }

    // doubleSided検出
    int twosided = 0;
    if (aiMat->Get(AI_MATKEY_TWOSIDED, twosided) == AI_SUCCESS && twosided) {
        material.doubleSided = true;
    }

    aiString texPath;
    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
        namespace fs = std::filesystem;
        std::string decodedPath = UrlDecode(texPath.C_Str());
        fs::path texturePath(decodedPath);
        material.diffuseTexturePath = texturePath.filename().string();
    }

    return material;
}

Mesh ProcessStaticMesh(const aiMesh* aiMesh, const aiScene* scene,
                       GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                       const std::string& baseDirectory,
                       const aiMatrix4x4& transform) {
    std::vector<Vertex> vertices;
    std::vector<uint32> indices;

    vertices.resize(aiMesh->mNumVertices);

    // マテリアルが指定するUVチャンネルを取得（glTFのtexCoord対応）
    int uvChannel = 0;
    if (aiMesh->mMaterialIndex < scene->mNumMaterials) {
        scene->mMaterials[aiMesh->mMaterialIndex]->Get(
            AI_MATKEY_UVWSRC(aiTextureType_DIFFUSE, 0), uvChannel);
    }

    // 法線用のトランスフォーム（逆転置行列）
    aiMatrix3x3 normalMatrix(transform);
    normalMatrix.Inverse();
    normalMatrix.Transpose();

    for (uint32 i = 0; i < aiMesh->mNumVertices; ++i) {
        Vertex& vertex = vertices[i];

        // トランスフォームを適用
        aiVector3D pos = transform * aiMesh->mVertices[i];
        
        // 座標変換: X座標を反転（右手→左手座標系）
        vertex.px = -pos.x;
        vertex.py = pos.y;
        vertex.pz = pos.z;

        if (aiMesh->HasNormals()) {
            // 法線にトランスフォームを適用（スケールに影響されないよう逆転置行列を使用）
            aiVector3D normal = normalMatrix * aiMesh->mNormals[i];
            normal.Normalize();
            // 法線もX成分を反転
            vertex.nx = -normal.x;
            vertex.ny = normal.y;
            vertex.nz = normal.z;
        } else {
            vertex.nx = 0.0f;
            vertex.ny = 1.0f;
            vertex.nz = 0.0f;
        }

        // マテリアルが指定するUVチャンネルを使用（glTFのtexCoord対応）
        if (aiMesh->HasTextureCoords(uvChannel)) {
            vertex.u = aiMesh->mTextureCoords[uvChannel][i].x;
            vertex.v = aiMesh->mTextureCoords[uvChannel][i].y;
        } else if (uvChannel != 0 && aiMesh->HasTextureCoords(0)) {
            // 指定チャンネルがなければTEXCOORD_0にフォールバック
            vertex.u = aiMesh->mTextureCoords[0][i].x;
            vertex.v = aiMesh->mTextureCoords[0][i].y;
        } else {
            vertex.u = 0.0f;
            vertex.v = 0.0f;
        }
    }

    for (uint32 i = 0; i < aiMesh->mNumFaces; ++i) {
        const aiFace& face = aiMesh->mFaces[i];
        if (face.mNumIndices == 3) {
            // X軸反転により巻き順を反転（0, 2, 1の順序）
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
        meshName = "static_mesh";
    }

    Mesh mesh;
    mesh.Create(graphics->GetDevice(), commandList, vertices, indices, meshName);

    if (aiMesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];
        MaterialData materialData = ConvertMaterial(aiMat, baseDirectory);
        uint32 srvIndex = graphics->AllocateSRVIndex();
        
        Logger::Debug("[StaticModelImporter] Material: {}, Texture: {}, SRV: {}",
                      materialData.name,
                      materialData.diffuseTexturePath.empty() ? "(none)" : materialData.diffuseTexturePath,
                      srvIndex);
        
        mesh.LoadMaterial(materialData, graphics, commandList, baseDirectory, srvIndex);
    }

    return mesh;
}

void ProcessNode(const aiNode* node, const aiScene* scene,
                 GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                 const std::string& baseDirectory,
                 std::vector<Mesh>& outMeshes,
                 const aiMatrix4x4& parentTransform = aiMatrix4x4()) {
    // 親のトランスフォームと現在のノードのトランスフォームを合成
    aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;
    
    Logger::Debug("[StaticModelImporter] ノード '{}' 処理中 (メッシュ: {}, 子ノード: {})", 
                  node->mName.C_Str(), node->mNumMeshes, node->mNumChildren);
    
    for (uint32 i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        // テクスチャのないメッシュをスキップ（glTFのデフォルトマテリアル＝地面プレーン等）
        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiString texPath;
            if (scene->mMaterials[mesh->mMaterialIndex]->GetTexture(
                    aiTextureType_DIFFUSE, 0, &texPath) != AI_SUCCESS) {
                Logger::Debug("[StaticModelImporter] メッシュ '{}' をスキップ (テクスチャなし)",
                              mesh->mName.C_Str());
                continue;
            }
        }

        outMeshes.push_back(ProcessStaticMesh(mesh, scene, graphics, commandList, baseDirectory, globalTransform));
    }

    for (uint32 i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene, graphics, commandList,
                   baseDirectory, outMeshes, globalTransform);
    }
}

} // anonymous namespace

StaticModelData StaticModelImporter::Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                          const std::string& filepath) {
    Assimp::Importer importer;

    // 静的モデル用のインポートフラグ
    // NOTE: aiProcess_PreTransformVerticesを使わない - メッシュがマージされてAABBが失われる
    unsigned int flags = aiProcess_Triangulate |
                        aiProcess_FlipUVs |
                        aiProcess_GenNormals |           // 法線がない場合は生成
                        aiProcess_CalcTangentSpace;      // タンジェント空間を計算

    const aiScene* scene = importer.ReadFile(filepath, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::Warning("[StaticModelImporter] ファイルをスキップ: {} ({})",
                     filepath, importer.GetErrorString());
        return {};
    }

    namespace fs = std::filesystem;
    const fs::path modelPath(filepath);
    const std::string baseDirectory = modelPath.parent_path().string();

    StaticModelData result;

    Logger::Info("[StaticModelImporter] シーンのメッシュ総数: {}", scene->mNumMeshes);
    
    ProcessNode(scene->mRootNode, scene, graphics, commandList,
               baseDirectory, result.meshes);

    // Calculate bounding box from all mesh bounds
    result.boundingBox.min = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    result.boundingBox.max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const auto& mesh : result.meshes) {
        Vector3 meshMin = mesh.GetBoundsMin();
        Vector3 meshMax = mesh.GetBoundsMax();

        if (meshMin.GetX() < result.boundingBox.min.GetX()) 
            result.boundingBox.min = Vector3(meshMin.GetX(), result.boundingBox.min.GetY(), result.boundingBox.min.GetZ());
        if (meshMin.GetY() < result.boundingBox.min.GetY()) 
            result.boundingBox.min = Vector3(result.boundingBox.min.GetX(), meshMin.GetY(), result.boundingBox.min.GetZ());
        if (meshMin.GetZ() < result.boundingBox.min.GetZ()) 
            result.boundingBox.min = Vector3(result.boundingBox.min.GetX(), result.boundingBox.min.GetY(), meshMin.GetZ());

        if (meshMax.GetX() > result.boundingBox.max.GetX()) 
            result.boundingBox.max = Vector3(meshMax.GetX(), result.boundingBox.max.GetY(), result.boundingBox.max.GetZ());
        if (meshMax.GetY() > result.boundingBox.max.GetY()) 
            result.boundingBox.max = Vector3(result.boundingBox.max.GetX(), meshMax.GetY(), result.boundingBox.max.GetZ());
        if (meshMax.GetZ() > result.boundingBox.max.GetZ()) 
            result.boundingBox.max = Vector3(result.boundingBox.max.GetX(), result.boundingBox.max.GetY(), meshMax.GetZ());
    }

    Logger::Info("[StaticModelImporter] 読み込み完了: メッシュ {}個, BoundingBox: ({:.2f},{:.2f},{:.2f}) - ({:.2f},{:.2f},{:.2f})", 
                 result.meshes.size(),
                 result.boundingBox.min.GetX(), result.boundingBox.min.GetY(), result.boundingBox.min.GetZ(),
                 result.boundingBox.max.GetX(), result.boundingBox.max.GetY(), result.boundingBox.max.GetZ());
    
    return result;
}

} // namespace UnoEngine
