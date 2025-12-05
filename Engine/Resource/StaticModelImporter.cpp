#include "pch.h"
#include "StaticModelImporter.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/Material.h"
#include "../Core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <Windows.h>
#include <iostream>
#include <cstdio>

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
                       const std::string& baseDirectory) {
    std::vector<Vertex> vertices;
    std::vector<uint32> indices;

    vertices.resize(aiMesh->mNumVertices);

    for (uint32 i = 0; i < aiMesh->mNumVertices; ++i) {
        Vertex& vertex = vertices[i];

        // 座標変換: X座標を反転（右手→左手座標系）
        vertex.px = -aiMesh->mVertices[i].x;
        vertex.py = aiMesh->mVertices[i].y;
        vertex.pz = aiMesh->mVertices[i].z;

        if (aiMesh->HasNormals()) {
            // 法線もX成分を反転
            vertex.nx = -aiMesh->mNormals[i].x;
            vertex.ny = aiMesh->mNormals[i].y;
            vertex.nz = aiMesh->mNormals[i].z;
        } else {
            vertex.nx = 0.0f;
            vertex.ny = 1.0f;
            vertex.nz = 0.0f;
        }

        // TEXCOORD_1があればそちらを優先（glTFのbaseColorTextureがtexCoord:1を指定している場合）
        // なければTEXCOORD_0を使用
        if (aiMesh->HasTextureCoords(1)) {
            vertex.u = aiMesh->mTextureCoords[1][i].x;
            vertex.v = aiMesh->mTextureCoords[1][i].y;
        } else if (aiMesh->HasTextureCoords(0)) {
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
        mesh.LoadMaterial(materialData, graphics, commandList, baseDirectory, srvIndex);
    }

    return mesh;
}

void ProcessNode(const aiNode* node, const aiScene* scene,
                 GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                 const std::string& baseDirectory,
                 std::vector<Mesh>& outMeshes) {
    Logger::Debug("[StaticModelImporter] ノード '{}' 処理中 (メッシュ: {}, 子ノード: {})", 
                  node->mName.C_Str(), node->mNumMeshes, node->mNumChildren);
    
    for (uint32 i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        outMeshes.push_back(ProcessStaticMesh(mesh, scene, graphics, commandList, baseDirectory));
    }

    for (uint32 i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene, graphics, commandList,
                   baseDirectory, outMeshes);
    }
}

} // anonymous namespace

StaticModelData StaticModelImporter::Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                          const std::string& filepath) {
    Assimp::Importer importer;

    // 静的モデル用のインポートフラグ
    unsigned int flags = aiProcess_Triangulate |
                        aiProcess_FlipUVs |
                        aiProcess_GenNormals |        // 法線がない場合は生成
                        aiProcess_CalcTangentSpace;   // タンジェント空間を計算

    const aiScene* scene = importer.ReadFile(filepath, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string errorMsg = "静的モデルファイルを読み込めませんでした\n\n";
        errorMsg += "Assimpエラー: " + std::string(importer.GetErrorString());
        LogImportError(errorMsg, filepath);
        throw std::runtime_error("Failed to load static model: " + filepath);
    }

    namespace fs = std::filesystem;
    const fs::path modelPath(filepath);
    const std::string baseDirectory = modelPath.parent_path().string();

    StaticModelData result;

    Logger::Info("[StaticModelImporter] シーンのメッシュ総数: {}", scene->mNumMeshes);
    
    ProcessNode(scene->mRootNode, scene, graphics, commandList,
               baseDirectory, result.meshes);

    Logger::Info("[StaticModelImporter] 読み込み完了: メッシュ {}個", result.meshes.size());
    
    return result;
}

} // namespace UnoEngine
