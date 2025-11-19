#define _CRT_SECURE_NO_WARNINGS
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON

#include "GltfLoader.h"
#include "../externals/tinygltf/stb_image.h"
#include "../externals/tinygltf/stb_image_write.h"
#include "../externals/tinygltf/json.hpp"
#include "../externals/tinygltf/tiny_gltf.h"
#include <stdexcept>
#include <iostream>

namespace UnoEngine {

ModelData GltfLoader::Load(GraphicsDevice* graphics,
                           ID3D12GraphicsCommandList* commandList,
                           const std::string& filepath) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    
    bool ret = false;
    
    // ファイル拡張子で.gltf/.glbを判定
    if (filepath.find(".glb") != std::string::npos) {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
    }
    
    if (!warn.empty()) {
        std::cout << "glTF Warning: " << warn << std::endl;
    }
    
    if (!err.empty()) {
        std::cerr << "glTF Error: " << err << std::endl;
    }
    
    if (!ret) {
        throw std::runtime_error("Failed to load glTF: " + filepath);
    }
    
    ModelData modelData;
    modelData.name = filepath;
    
    // 各メッシュをロード
    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
        const auto& gltfMesh = model.meshes[meshIdx];
        
        // 各プリミティブ（サブメッシュ）をロード
        for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); ++primIdx) {
            const auto& primitive = gltfMesh.primitives[primIdx];
            
            // TRIANGLES のみサポート
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                continue;
            }
            
            // 頂点データとインデックスデータを抽出
            auto vertices = ExtractVertices(&model, static_cast<int>(meshIdx), static_cast<int>(primIdx));
            auto indices = ExtractIndices(&model, static_cast<int>(meshIdx), static_cast<int>(primIdx));
            
            if (vertices.empty() || indices.empty()) {
                continue;
            }
            
            // Meshオブジェクトを作成
            auto mesh = MakeUnique<Mesh>();
            std::string meshName = gltfMesh.name.empty() 
                ? "mesh_" + std::to_string(meshIdx) + "_" + std::to_string(primIdx)
                : gltfMesh.name + "_" + std::to_string(primIdx);
            
            mesh->Create(graphics->GetDevice(), commandList, vertices, indices, meshName);
            
            // マテリアルが存在する場合はロード
            if (primitive.material >= 0) {
                auto materialData = ExtractMaterial(&model, primitive.material);
                
                // ベースディレクトリを取得
                std::string baseDir = filepath.substr(0, filepath.find_last_of("/\\"));
                
                mesh->LoadMaterial(materialData, graphics, commandList, baseDir, 
                                  static_cast<uint32>(modelData.meshes.size()));
            }
            
            modelData.meshes.push_back(std::move(mesh));
        }
    }
    
    return modelData;
}

std::vector<Vertex> GltfLoader::ExtractVertices(const void* modelPtr, int meshIndex, int primitiveIndex) {
    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    const auto& primitive = model.meshes[meshIndex].primitives[primitiveIndex];
    
    std::vector<Vertex> vertices;
    
    // POSITIONアクセサを取得
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        return vertices;
    }
    
    const auto& posAccessor = model.accessors[posIt->second];
    const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
    const auto& posBuffer = model.buffers[posBufferView.buffer];
    
    const float* positions = reinterpret_cast<const float*>(
        &posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);
    
    size_t vertexCount = posAccessor.count;
    vertices.resize(vertexCount);
    
    // 位置データをコピー
    size_t posStride = posBufferView.byteStride 
        ? posBufferView.byteStride / sizeof(float) 
        : 3;
    
    for (size_t i = 0; i < vertexCount; ++i) {
        vertices[i].px = positions[i * posStride + 0];
        vertices[i].py = positions[i * posStride + 1];
        vertices[i].pz = positions[i * posStride + 2];
    }
    
    // NORMALアクセサを取得（オプション）
    auto normIt = primitive.attributes.find("NORMAL");
    if (normIt != primitive.attributes.end()) {
        const auto& normAccessor = model.accessors[normIt->second];
        const auto& normBufferView = model.bufferViews[normAccessor.bufferView];
        const auto& normBuffer = model.buffers[normBufferView.buffer];
        
        const float* normals = reinterpret_cast<const float*>(
            &normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
        
        size_t normStride = normBufferView.byteStride 
            ? normBufferView.byteStride / sizeof(float) 
            : 3;
        
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].nx = normals[i * normStride + 0];
            vertices[i].ny = normals[i * normStride + 1];
            vertices[i].nz = normals[i * normStride + 2];
        }
    } else {
        // 法線がない場合はゼロで初期化
        for (auto& v : vertices) {
            v.nx = v.ny = v.nz = 0.0f;
        }
    }
    
    // TEXCOORD_0アクセサを取得（オプション）
    auto uvIt = primitive.attributes.find("TEXCOORD_0");
    if (uvIt != primitive.attributes.end()) {
        const auto& uvAccessor = model.accessors[uvIt->second];
        const auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
        const auto& uvBuffer = model.buffers[uvBufferView.buffer];
        
        const float* uvs = reinterpret_cast<const float*>(
            &uvBuffer.data[uvBufferView.byteOffset + uvAccessor.byteOffset]);
        
        size_t uvStride = uvBufferView.byteStride 
            ? uvBufferView.byteStride / sizeof(float) 
            : 2;
        
        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].u = uvs[i * uvStride + 0];
            vertices[i].v = uvs[i * uvStride + 1];
        }
    } else {
        // UVがない場合はゼロで初期化
        for (auto& v : vertices) {
            v.u = v.v = 0.0f;
        }
    }
    
    return vertices;
}

std::vector<uint32> GltfLoader::ExtractIndices(const void* modelPtr, int meshIndex, int primitiveIndex) {
    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    const auto& primitive = model.meshes[meshIndex].primitives[primitiveIndex];
    
    std::vector<uint32> indices;
    
    if (primitive.indices < 0) {
        return indices;
    }
    
    const auto& accessor = model.accessors[primitive.indices];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    
    const unsigned char* data = &buffer.data[bufferView.byteOffset + accessor.byteOffset];
    
    indices.resize(accessor.count);
    
    // インデックスのタイプに応じて読み込み
    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = static_cast<uint32>(ptr[i]);
            }
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data);
            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = static_cast<uint32>(ptr[i]);
            }
            break;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            const uint32_t* ptr = reinterpret_cast<const uint32_t*>(data);
            for (size_t i = 0; i < accessor.count; ++i) {
                indices[i] = ptr[i];
            }
            break;
        }
        default:
            indices.clear();
            break;
    }
    
    return indices;
}

MaterialData GltfLoader::ExtractMaterial(const void* modelPtr, int materialIndex) {
    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    const auto& gltfMat = model.materials[materialIndex];
    
    MaterialData matData;
    matData.name = gltfMat.name;
    
    // PBRメタリックラフネス
    const auto& pbr = gltfMat.pbrMetallicRoughness;
    
    // Base Color
    if (pbr.baseColorFactor.size() >= 3) {
        matData.diffuse[0] = static_cast<float>(pbr.baseColorFactor[0]);
        matData.diffuse[1] = static_cast<float>(pbr.baseColorFactor[1]);
        matData.diffuse[2] = static_cast<float>(pbr.baseColorFactor[2]);
    } else {
        matData.diffuse[0] = matData.diffuse[1] = matData.diffuse[2] = 1.0f;
    }
    
    // Base Color Texture
    if (pbr.baseColorTexture.index >= 0) {
        const auto& texture = model.textures[pbr.baseColorTexture.index];
        if (texture.source >= 0) {
            const auto& image = model.images[texture.source];
            matData.diffuseTexturePath = image.uri;
        }
    }
    
    // デフォルト値
    matData.ambient[0] = matData.ambient[1] = matData.ambient[2] = 0.2f;
    matData.specular[0] = matData.specular[1] = matData.specular[2] = 0.5f;
    matData.shininess = 32.0f;
    
    return matData;
}

} // namespace UnoEngine
