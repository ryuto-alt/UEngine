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
#include "../Rendering/Skeleton.h"
#include "../Rendering/Animation.h"
#include <stdexcept>
#include <iostream>
#include <unordered_map>

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

    std::cout << "glTF: Loading " << filepath << std::endl;
    std::cout << "glTF: Found " << model.meshes.size() << " meshes" << std::endl;
    std::cout << "glTF: Found " << model.skins.size() << " skins" << std::endl;
    std::cout << "glTF: Found " << model.animations.size() << " animations" << std::endl;

    if (!model.skins.empty()) {
        modelData.hasSkin = true;
        modelData.skeleton = ExtractSkeleton(&model, 0);
        std::cout << "glTF: Loaded skeleton with " << modelData.skeleton->GetJointCount() << " joints" << std::endl;
    }

    if (!model.animations.empty()) {
        modelData.animations = ExtractAnimations(&model);
        std::cout << "glTF: Loaded " << modelData.animations.size() << " animations" << std::endl;
    }

    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
        const auto& gltfMesh = model.meshes[meshIdx];

        for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); ++primIdx) {
            const auto& primitive = gltfMesh.primitives[primIdx];

            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                continue;
            }

            bool hasSkinning = HasSkinningAttributes(&model, static_cast<int>(meshIdx), static_cast<int>(primIdx));
            auto indices = ExtractIndices(&model, static_cast<int>(meshIdx), static_cast<int>(primIdx));

            if (indices.empty()) {
                continue;
            }

            std::string meshName = gltfMesh.name.empty()
                ? "mesh_" + std::to_string(meshIdx) + "_" + std::to_string(primIdx)
                : gltfMesh.name + "_" + std::to_string(primIdx);

            MaterialData materialData;
            if (primitive.material >= 0) {
                materialData = ExtractMaterial(&model, primitive.material);
            } else {
                materialData.name = "DefaultWhite";
                materialData.diffuse[0] = materialData.diffuse[1] = materialData.diffuse[2] = 1.0f;
                materialData.ambient[0] = materialData.ambient[1] = materialData.ambient[2] = 0.8f;
                materialData.specular[0] = materialData.specular[1] = materialData.specular[2] = 0.5f;
                materialData.shininess = 32.0f;
                materialData.diffuseTexturePath = "white1x1.png";
            }

            std::string baseDir = filepath.substr(0, filepath.find_last_of("/\\"));

            if (hasSkinning) {
                auto vertices = ExtractSkinnedVertices(&model, static_cast<int>(meshIdx), static_cast<int>(primIdx));
                if (vertices.empty()) continue;

                auto mesh = MakeUnique<SkinnedMesh>();
                mesh->Create(graphics->GetDevice(), commandList, vertices, indices, meshName);
                mesh->LoadMaterial(materialData, graphics, commandList, baseDir,
                                  static_cast<uint32>(modelData.skinnedMeshes.size()));

                std::cout << "glTF: SkinnedMesh " << meshName << " - vertices: " << vertices.size()
                         << ", indices: " << indices.size() << std::endl;

                modelData.skinnedMeshes.push_back(std::move(mesh));
            } else {
                auto vertices = ExtractVertices(&model, static_cast<int>(meshIdx), static_cast<int>(primIdx));
                if (vertices.empty()) continue;

                auto mesh = MakeUnique<Mesh>();
                mesh->Create(graphics->GetDevice(), commandList, vertices, indices, meshName);
                mesh->LoadMaterial(materialData, graphics, commandList, baseDir,
                                  static_cast<uint32>(modelData.meshes.size()));

                std::cout << "glTF: Mesh " << meshName << " - vertices: " << vertices.size()
                         << ", indices: " << indices.size() << std::endl;

                modelData.meshes.push_back(std::move(mesh));
            }
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
        // 法線がない場合は上向き（Y+）で初期化
        for (auto& v : vertices) {
            v.nx = 0.0f;
            v.ny = 1.0f;
            v.nz = 0.0f;
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
    
    // デフォルト値（ambientを明るく設定）
    matData.ambient[0] = matData.ambient[1] = matData.ambient[2] = 0.8f;
    matData.specular[0] = matData.specular[1] = matData.specular[2] = 0.5f;
    matData.shininess = 32.0f;
    
    return matData;
}

bool GltfLoader::HasSkinningAttributes(const void* modelPtr, int meshIndex, int primitiveIndex) {
    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    const auto& primitive = model.meshes[meshIndex].primitives[primitiveIndex];

    bool hasJoints = primitive.attributes.find("JOINTS_0") != primitive.attributes.end();
    bool hasWeights = primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end();

    return hasJoints && hasWeights;
}

std::vector<VertexSkinned> GltfLoader::ExtractSkinnedVertices(const void* modelPtr, int meshIndex, int primitiveIndex) {
    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    const auto& primitive = model.meshes[meshIndex].primitives[primitiveIndex];

    std::vector<VertexSkinned> vertices;

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

    size_t posStride = posBufferView.byteStride
        ? posBufferView.byteStride / sizeof(float)
        : 3;

    for (size_t i = 0; i < vertexCount; ++i) {
        vertices[i].px = positions[i * posStride + 0];
        vertices[i].py = positions[i * posStride + 1];
        vertices[i].pz = positions[i * posStride + 2];
    }

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
        for (auto& v : vertices) {
            v.nx = 0.0f;
            v.ny = 1.0f;
            v.nz = 0.0f;
        }
    }

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
        for (auto& v : vertices) {
            v.u = v.v = 0.0f;
        }
    }

    auto jointsIt = primitive.attributes.find("JOINTS_0");
    if (jointsIt != primitive.attributes.end()) {
        const auto& jointsAccessor = model.accessors[jointsIt->second];
        const auto& jointsBufferView = model.bufferViews[jointsAccessor.bufferView];
        const auto& jointsBuffer = model.buffers[jointsBufferView.buffer];

        const unsigned char* data = &jointsBuffer.data[jointsBufferView.byteOffset + jointsAccessor.byteOffset];

        if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            const uint8_t* joints = reinterpret_cast<const uint8_t*>(data);
            size_t stride = jointsBufferView.byteStride ? jointsBufferView.byteStride : 4;
            for (size_t i = 0; i < vertexCount; ++i) {
                vertices[i].joints[0] = joints[i * stride + 0];
                vertices[i].joints[1] = joints[i * stride + 1];
                vertices[i].joints[2] = joints[i * stride + 2];
                vertices[i].joints[3] = joints[i * stride + 3];
            }
        } else if (jointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t* joints = reinterpret_cast<const uint16_t*>(data);
            size_t stride = jointsBufferView.byteStride ? jointsBufferView.byteStride / sizeof(uint16_t) : 4;
            for (size_t i = 0; i < vertexCount; ++i) {
                vertices[i].joints[0] = joints[i * stride + 0];
                vertices[i].joints[1] = joints[i * stride + 1];
                vertices[i].joints[2] = joints[i * stride + 2];
                vertices[i].joints[3] = joints[i * stride + 3];
            }
        }
    } else {
        for (auto& v : vertices) {
            v.joints[0] = v.joints[1] = v.joints[2] = v.joints[3] = 0;
        }
    }

    auto weightsIt = primitive.attributes.find("WEIGHTS_0");
    if (weightsIt != primitive.attributes.end()) {
        const auto& weightsAccessor = model.accessors[weightsIt->second];
        const auto& weightsBufferView = model.bufferViews[weightsAccessor.bufferView];
        const auto& weightsBuffer = model.buffers[weightsBufferView.buffer];

        const float* weights = reinterpret_cast<const float*>(
            &weightsBuffer.data[weightsBufferView.byteOffset + weightsAccessor.byteOffset]);

        size_t stride = weightsBufferView.byteStride
            ? weightsBufferView.byteStride / sizeof(float)
            : 4;

        for (size_t i = 0; i < vertexCount; ++i) {
            vertices[i].weights[0] = weights[i * stride + 0];
            vertices[i].weights[1] = weights[i * stride + 1];
            vertices[i].weights[2] = weights[i * stride + 2];
            vertices[i].weights[3] = weights[i * stride + 3];
        }
    } else {
        for (auto& v : vertices) {
            v.weights[0] = 1.0f;
            v.weights[1] = v.weights[2] = v.weights[3] = 0.0f;
        }
    }

    return vertices;
}

UniquePtr<Skeleton> GltfLoader::ExtractSkeleton(const void* modelPtr, int skinIndex) {
    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);

    if (skinIndex < 0 || skinIndex >= static_cast<int>(model.skins.size())) {
        return nullptr;
    }

    const auto& skin = model.skins[skinIndex];
    auto skeleton = MakeUnique<Skeleton>();

    std::vector<Matrix4x4> inverseBindMatrices;
    if (skin.inverseBindMatrices >= 0) {
        const auto& accessor = model.accessors[skin.inverseBindMatrices];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];

        const float* matrices = reinterpret_cast<const float*>(
            &buffer.data[bufferView.byteOffset + accessor.byteOffset]);

        inverseBindMatrices.resize(accessor.count);
        for (size_t i = 0; i < accessor.count; ++i) {
            std::memcpy(&inverseBindMatrices[i], &matrices[i * 16], sizeof(Matrix4x4));
        }
    }

    std::unordered_map<int32, int32> nodeToJointIndex;
    for (size_t i = 0; i < skin.joints.size(); ++i) {
        nodeToJointIndex[skin.joints[i]] = static_cast<int32>(i);
    }

    for (size_t i = 0; i < skin.joints.size(); ++i) {
        int nodeIdx = skin.joints[i];
        const auto& node = model.nodes[nodeIdx];

        Joint joint;
        joint.name = node.name;

        if (node.translation.size() == 3) {
            joint.translation = Vector3(
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2]));
        }

        if (node.rotation.size() == 4) {
            joint.rotation = Quaternion(
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2]),
                static_cast<float>(node.rotation[3]));
        }

        if (node.scale.size() == 3) {
            joint.scale = Vector3(
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2]));
        }

        for (int childIdx : node.children) {
            auto it = nodeToJointIndex.find(childIdx);
            if (it != nodeToJointIndex.end()) {
                int32 childJointIdx = it->second;
                if (childJointIdx < static_cast<int32>(skin.joints.size())) {
                    if (i < skin.joints.size()) {
                        joint.parentIndex = -1;
                    }
                }
            }
        }

        if (i < inverseBindMatrices.size()) {
            joint.inverseBindMatrix = inverseBindMatrices[i];
        }

        skeleton->AddJoint(joint);
    }

    for (size_t i = 0; i < skin.joints.size(); ++i) {
        int nodeIdx = skin.joints[i];
        const auto& node = model.nodes[nodeIdx];

        for (int childIdx : node.children) {
            auto it = nodeToJointIndex.find(childIdx);
            if (it != nodeToJointIndex.end()) {
                int32 childJointIdx = it->second;
                if (childJointIdx < static_cast<int32>(skeleton->GetJoints().size())) {
                    skeleton->GetJoints()[childJointIdx].parentIndex = static_cast<int32>(i);
                }
            }
        }
    }

    skeleton->ComputeGlobalTransforms();
    return skeleton;
}

std::vector<UniquePtr<Animation>> GltfLoader::ExtractAnimations(const void* modelPtr) {
    const auto& model = *static_cast<const tinygltf::Model*>(modelPtr);
    std::vector<UniquePtr<Animation>> animations;

    for (const auto& gltfAnim : model.animations) {
        auto animation = MakeUnique<Animation>();
        animation->SetName(gltfAnim.name.empty() ? "Animation" : gltfAnim.name);

        for (const auto& gltfSampler : gltfAnim.samplers) {
            AnimationSampler sampler;

            if (gltfSampler.interpolation == "LINEAR") {
                sampler.interpolation = InterpolationType::Linear;
            } else if (gltfSampler.interpolation == "STEP") {
                sampler.interpolation = InterpolationType::Step;
            } else if (gltfSampler.interpolation == "CUBICSPLINE") {
                sampler.interpolation = InterpolationType::CubicSpline;
            }

            const auto& inputAccessor = model.accessors[gltfSampler.input];
            const auto& inputBufferView = model.bufferViews[inputAccessor.bufferView];
            const auto& inputBuffer = model.buffers[inputBufferView.buffer];

            const float* times = reinterpret_cast<const float*>(
                &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset]);

            sampler.times.assign(times, times + inputAccessor.count);

            const auto& outputAccessor = model.accessors[gltfSampler.output];
            const auto& outputBufferView = model.bufferViews[outputAccessor.bufferView];
            const auto& outputBuffer = model.buffers[outputBufferView.buffer];

            const float* outputs = reinterpret_cast<const float*>(
                &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset]);

            if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
                for (size_t i = 0; i < outputAccessor.count; ++i) {
                    sampler.outputVec3.push_back(Vector3(outputs[i * 3 + 0], outputs[i * 3 + 1], outputs[i * 3 + 2]));
                }
            } else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
                for (size_t i = 0; i < outputAccessor.count; ++i) {
                    sampler.outputQuat.push_back(Quaternion(outputs[i * 4 + 0], outputs[i * 4 + 1], outputs[i * 4 + 2], outputs[i * 4 + 3]));
                }
            }

            animation->AddSampler(sampler);
        }

        for (const auto& gltfChannel : gltfAnim.channels) {
            AnimationChannel channel;
            channel.samplerIndex = gltfChannel.sampler;
            channel.targetJointIndex = gltfChannel.target_node;

            if (gltfChannel.target_path == "translation") {
                channel.path = AnimationPath::Translation;
            } else if (gltfChannel.target_path == "rotation") {
                channel.path = AnimationPath::Rotation;
            } else if (gltfChannel.target_path == "scale") {
                channel.path = AnimationPath::Scale;
            }

            animation->AddChannel(channel);
        }

        animations.push_back(std::move(animation));
    }

    return animations;
}

} // namespace UnoEngine
