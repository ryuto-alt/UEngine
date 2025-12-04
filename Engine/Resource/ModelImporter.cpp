#include "pch.h"
#include "ModelImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <Windows.h>
#include <iostream>

namespace UnoEngine {

namespace {

void LogImportError(const std::string& message, const std::string& file) {
    std::string fullMessage = "[モデル読み込みエラー]\n\n" + message + "\n\nファイル: " + file;
    std::cerr << fullMessage << std::endl;
    OutputDebugStringA((fullMessage + "\n").c_str());

    int wideSize = MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), -1, nullptr, 0);
    std::wstring wideMessage(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, fullMessage.c_str(), -1, &wideMessage[0], wideSize);

    MessageBoxW(nullptr, wideMessage.c_str(), L"モデル読み込みエラー", MB_OK | MB_ICONERROR);
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

    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
        material.emissive[0] = color.r;
        material.emissive[1] = color.g;
        material.emissive[2] = color.b;
    }

    float shininess = 0.0f;
    if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
        material.shininess = shininess;
    }

    float opacity = 1.0f;
    if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
        material.opacity = opacity;
    }

    aiString texPath;
    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
        namespace fs = std::filesystem;
        fs::path texturePath(texPath.C_Str());

        // 常にファイル名のみを使用（MTLファイルのパスが壊れている場合に対応）
        // テクスチャはbaseDirectoryと結合されるため、ファイル名だけで十分
        material.diffuseTexturePath = texturePath.filename().string();
    }

    return material;
}

Mesh ProcessMesh(const aiMesh* aiMesh, const aiScene* scene,
                 GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                 const std::string& baseDirectory) {
    std::vector<Vertex> vertices;
    std::vector<uint32> indices;

    vertices.reserve(aiMesh->mNumVertices);

    for (unsigned int i = 0; i < aiMesh->mNumVertices; ++i) {
        Vertex vertex;

        vertex.px = aiMesh->mVertices[i].x;
        vertex.py = aiMesh->mVertices[i].y;
        vertex.pz = aiMesh->mVertices[i].z;

        if (aiMesh->HasNormals()) {
            vertex.nx = aiMesh->mNormals[i].x;
            vertex.ny = aiMesh->mNormals[i].y;
            vertex.nz = aiMesh->mNormals[i].z;
        } else {
            vertex.nx = 0.0f;
            vertex.ny = 1.0f;
            vertex.nz = 0.0f;
        }

        if (aiMesh->HasTextureCoords(0)) {
            vertex.u = aiMesh->mTextureCoords[0][i].x;
            vertex.v = aiMesh->mTextureCoords[0][i].y;
        } else {
            vertex.u = 0.0f;
            vertex.v = 0.0f;
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < aiMesh->mNumFaces; ++i) {
        const aiFace& face = aiMesh->mFaces[i];

        if (face.mNumIndices != 3) {
            std::stringstream detailMsg;
            detailMsg << "【問題】\n";
            detailMsg << "このメッシュには" << face.mNumIndices << "角形のポリゴンが含まれています。\n";
            detailMsg << "このエンジンは三角形ポリゴンのみ対応しています。\n\n";
            detailMsg << "【解決方法】\n";
            detailMsg << "メッシュを三角面化：\n\n";
            detailMsg << "■ Blender の場合:\n";
            detailMsg << "  1. すべて選択 (A キー)\n";
            detailMsg << "  2. 右クリック → Triangulate Faces\n";
            detailMsg << "  または Ctrl+T\n\n";
            detailMsg << "■ Maya の場合:\n";
            detailMsg << "  Mesh → Triangulate\n\n";
            detailMsg << "■ 3ds Max の場合:\n";
            detailMsg << "  Edit Poly → Turn to Triangles";

            throw std::runtime_error(detailMsg.str());
        }

        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    std::string meshName = aiMesh->mName.C_Str();
    if (meshName.empty()) {
        meshName = "unnamed_mesh";
    }

    Mesh mesh;
    mesh.Create(graphics->GetDevice(), commandList, vertices, indices, meshName);

    if (aiMesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];
        MaterialData materialData = ConvertMaterial(aiMat, baseDirectory);
        mesh.LoadMaterial(materialData, graphics, commandList, baseDirectory, 0);
    }

    char debugMsg[512];
    sprintf_s(debugMsg, "Mesh Loaded: %s - %zu vertices, %zu indices\n",
             meshName.c_str(), vertices.size(), indices.size());
    OutputDebugStringA(debugMsg);

    return mesh;
}

void ProcessNode(const aiNode* node, const aiScene* scene,
                 GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                 const std::string& baseDirectory, std::vector<Mesh>& outMeshes) {
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];
        outMeshes.push_back(ProcessMesh(aiMesh, scene, graphics, commandList, baseDirectory));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene, graphics, commandList, baseDirectory, outMeshes);
    }
}

} // anonymous namespace

std::vector<Mesh> ModelImporter::Load(GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                      const std::string& filepath) {
    Assimp::Importer importer;

    unsigned int flags = aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenNormals;

    const aiScene* scene = importer.ReadFile(filepath, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string errorMsg = "モデルファイルを読み込めませんでした\n\n";
        errorMsg += "ファイルパス:\n" + filepath + "\n\n";
        errorMsg += "Assimpエラー: " + std::string(importer.GetErrorString()) + "\n\n";
        errorMsg += "確認してください:\n";
        errorMsg += "  1. ファイルが存在するか\n";
        errorMsg += "  2. ファイルパスが正しいか\n";
        errorMsg += "  3. ファイル形式が対応しているか (OBJ, FBX, glTF等)";
        LogImportError(errorMsg, filepath);
        throw std::runtime_error("Failed to load model: " + filepath);
    }

    namespace fs = std::filesystem;
    const fs::path modelPath(filepath);
    const std::string baseDirectory = modelPath.parent_path().string();

    std::vector<Mesh> meshes;
    ProcessNode(scene->mRootNode, scene, graphics, commandList, baseDirectory, meshes);

    if (meshes.empty()) {
        std::stringstream errorMsg;
        errorMsg << "【問題】\n";
        errorMsg << "モデルファイルにジオメトリ（形状データ）が含まれていません\n\n";
        errorMsg << "【確認してください】\n";
        errorMsg << "  - ファイルにメッシュが含まれているか\n";
        errorMsg << "  - エクスポート設定が正しいか";
        LogImportError(errorMsg.str(), filepath);
        throw std::runtime_error("Model file contains no geometry");
    }

    const size_t lastSlash = filepath.find_last_of("/\\");
    const std::string name = (lastSlash != std::string::npos)
        ? filepath.substr(lastSlash + 1)
        : filepath;

    char debugMsg[512];
    sprintf_s(debugMsg, "Model Loaded: %s - %zu meshes, %u materials\n",
             name.c_str(), meshes.size(), scene->mNumMaterials);
    OutputDebugStringA(debugMsg);

    return meshes;
}

} // namespace UnoEngine
