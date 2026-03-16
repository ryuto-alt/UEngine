#include "pch.h"
#include "ModelCache.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Core/Logger.h"
#include <filesystem>
#include <functional>
#include <chrono>

namespace UnoEngine {

bool ModelCache::enabled_ = true;
std::string ModelCache::cacheDirectory_ = ".cache/models";

// ========================================
// BinaryWriter
// ========================================
ModelCache::BinaryWriter::BinaryWriter(const std::string& path)
    : stream_(path, std::ios::binary) {}
ModelCache::BinaryWriter::~BinaryWriter() = default;
bool ModelCache::BinaryWriter::IsOpen() const { return stream_.is_open() && stream_.good(); }
void ModelCache::BinaryWriter::Write(const void* data, size_t size) { stream_.write(static_cast<const char*>(data), size); }
void ModelCache::BinaryWriter::WriteU32(uint32 val) { Write(&val, sizeof(val)); }
void ModelCache::BinaryWriter::WriteI32(int32 val) { Write(&val, sizeof(val)); }
void ModelCache::BinaryWriter::WriteF32(float val) { Write(&val, sizeof(val)); }
void ModelCache::BinaryWriter::WriteBool(bool val) { uint8_t b = val ? 1 : 0; Write(&b, 1); }
void ModelCache::BinaryWriter::WriteString(const std::string& str) {
    WriteU32(static_cast<uint32>(str.size()));
    if (!str.empty()) Write(str.data(), str.size());
}
void ModelCache::BinaryWriter::WriteVector3(const Vector3& v) {
    WriteF32(v.GetX()); WriteF32(v.GetY()); WriteF32(v.GetZ());
}
void ModelCache::BinaryWriter::WriteMatrix(const Matrix4x4& m) {
    // Matrix4x4 is 64 bytes (float[4][4]) - write as raw memory
    Write(&m, sizeof(Matrix4x4));
}
void ModelCache::BinaryWriter::WriteQuaternion(const Quaternion& q) {
    WriteF32(q.GetX()); WriteF32(q.GetY()); WriteF32(q.GetZ()); WriteF32(q.GetW());
}

// ========================================
// BinaryReader
// ========================================
ModelCache::BinaryReader::BinaryReader(const std::string& path)
    : stream_(path, std::ios::binary) {}
ModelCache::BinaryReader::~BinaryReader() = default;
bool ModelCache::BinaryReader::IsOpen() const { return stream_.is_open() && stream_.good(); }
void ModelCache::BinaryReader::Read(void* data, size_t size) { stream_.read(static_cast<char*>(data), size); }
uint32 ModelCache::BinaryReader::ReadU32() { uint32 val; Read(&val, sizeof(val)); return val; }
int32 ModelCache::BinaryReader::ReadI32() { int32 val; Read(&val, sizeof(val)); return val; }
float ModelCache::BinaryReader::ReadF32() { float val; Read(&val, sizeof(val)); return val; }
bool ModelCache::BinaryReader::ReadBool() { uint8_t b; Read(&b, 1); return b != 0; }
std::string ModelCache::BinaryReader::ReadString() {
    uint32 len = ReadU32();
    if (len == 0) return {};
    std::string str(len, '\0');
    Read(str.data(), len);
    return str;
}
Vector3 ModelCache::BinaryReader::ReadVector3() {
    float x = ReadF32(), y = ReadF32(), z = ReadF32();
    return Vector3(x, y, z);
}
Matrix4x4 ModelCache::BinaryReader::ReadMatrix() {
    Matrix4x4 m;
    Read(&m, sizeof(Matrix4x4));
    return m;
}
Quaternion ModelCache::BinaryReader::ReadQuaternion() {
    float x = ReadF32(), y = ReadF32(), z = ReadF32(), w = ReadF32();
    return Quaternion(x, y, z, w);
}

// ========================================
// Cache path
// ========================================
std::string ModelCache::GetCachePath(const std::string& sourcePath, const std::string& suffix) {
    namespace fs = std::filesystem;
    size_t hash = std::hash<std::string>{}(fs::absolute(sourcePath).string());
    std::string stem = fs::path(sourcePath).stem().string();
    return cacheDirectory_ + "/" + stem + "_" + std::to_string(hash) + suffix;
}

bool ModelCache::IsCacheValid(const std::string& sourcePath, const std::string& cachePath) {
    namespace fs = std::filesystem;
    if (!fs::exists(cachePath)) return false;
    auto sourceTime = fs::last_write_time(sourcePath);
    auto cacheTime = fs::last_write_time(cachePath);
    return cacheTime >= sourceTime;
}

// ========================================
// Static Model Cache
// ========================================
void ModelCache::SaveStaticModel(const std::string& sourcePath, const StaticModelData& data) {
    namespace fs = std::filesystem;

    std::string cachePath = GetCachePath(sourcePath, ".smcache");

    fs::path cacheDir(cacheDirectory_);
    if (!fs::exists(cacheDir)) {
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
        if (ec) {
            Logger::Warning("[ModelCache] キャッシュディレクトリ作成失敗: {}", ec.message());
            return;
        }
    }

    BinaryWriter writer(cachePath);
    if (!writer.IsOpen()) {
        Logger::Warning("[ModelCache] キャッシュファイル作成失敗: {}", cachePath);
        return;
    }

    // Header
    writer.WriteU32(CACHE_MAGIC);
    writer.WriteU32(CACHE_VERSION_STATIC);

    // Bounding box
    writer.WriteVector3(data.boundingBox.min);
    writer.WriteVector3(data.boundingBox.max);

    // Mesh count
    writer.WriteU32(static_cast<uint32>(data.meshes.size()));

    for (const auto& mesh : data.meshes) {
        // Mesh name
        writer.WriteString(mesh.GetName());

        // Vertices
        const auto& verts = mesh.GetVertices();
        writer.WriteU32(static_cast<uint32>(verts.size()));
        if (!verts.empty()) {
            writer.Write(verts.data(), verts.size() * sizeof(Vertex));
        }

        // Indices
        const auto& inds = mesh.GetIndices();
        writer.WriteU32(static_cast<uint32>(inds.size()));
        if (!inds.empty()) {
            writer.Write(inds.data(), inds.size() * sizeof(uint32));
        }

        // Material data
        bool hasMaterial = mesh.HasMaterial();
        writer.WriteBool(hasMaterial);
        if (hasMaterial) {
            const auto& mat = mesh.GetMaterial()->GetData();
            writer.WriteString(mat.name);
            writer.Write(mat.ambient, sizeof(mat.ambient));
            writer.Write(mat.diffuse, sizeof(mat.diffuse));
            writer.Write(mat.specular, sizeof(mat.specular));
            writer.Write(mat.emissive, sizeof(mat.emissive));
            writer.WriteF32(mat.shininess);
            writer.WriteF32(mat.opacity);
            writer.WriteString(mat.diffuseTexturePath);
            writer.WriteF32(mat.metallic);
            writer.WriteF32(mat.roughness);
            writer.Write(mat.albedo, sizeof(mat.albedo));
            writer.WriteBool(mat.useAlphaClip);
            writer.WriteF32(mat.alphaClipThreshold);
            writer.WriteBool(mat.useAlphaBlend);
            writer.WriteBool(mat.doubleSided);
        }
    }

    Logger::Debug("[ModelCache] 静的モデルキャッシュ保存完了: {}", cachePath);
}

bool ModelCache::TryLoadStaticModel(const std::string& sourcePath,
                                     GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                     StaticModelData& outData) {
    std::string cachePath = GetCachePath(sourcePath, ".smcache");
    if (!IsCacheValid(sourcePath, cachePath)) return false;

    BinaryReader reader(cachePath);
    if (!reader.IsOpen()) return false;

    // Header check
    uint32 magic = reader.ReadU32();
    uint32 version = reader.ReadU32();
    if (magic != CACHE_MAGIC || version != CACHE_VERSION_STATIC) return false;

    // Bounding box
    outData.boundingBox.min = reader.ReadVector3();
    outData.boundingBox.max = reader.ReadVector3();

    // Base directory for texture loading
    namespace fs = std::filesystem;
    std::string baseDirectory = fs::path(sourcePath).parent_path().string();

    // Meshes
    uint32 meshCount = reader.ReadU32();
    outData.meshes.reserve(meshCount);

    for (uint32 i = 0; i < meshCount; ++i) {
        std::string meshName = reader.ReadString();

        // Vertices
        uint32 vertCount = reader.ReadU32();
        std::vector<Vertex> vertices(vertCount);
        if (vertCount > 0) {
            reader.Read(vertices.data(), vertCount * sizeof(Vertex));
        }

        // Indices
        uint32 indexCount = reader.ReadU32();
        std::vector<uint32> indices(indexCount);
        if (indexCount > 0) {
            reader.Read(indices.data(), indexCount * sizeof(uint32));
        }

        Mesh mesh;
        mesh.Create(graphics->GetDevice(), commandList, vertices, indices, meshName);

        // Material
        bool hasMaterial = reader.ReadBool();
        if (hasMaterial) {
            MaterialData mat;
            mat.name = reader.ReadString();
            reader.Read(mat.ambient, sizeof(mat.ambient));
            reader.Read(mat.diffuse, sizeof(mat.diffuse));
            reader.Read(mat.specular, sizeof(mat.specular));
            reader.Read(mat.emissive, sizeof(mat.emissive));
            mat.shininess = reader.ReadF32();
            mat.opacity = reader.ReadF32();
            mat.diffuseTexturePath = reader.ReadString();
            mat.metallic = reader.ReadF32();
            mat.roughness = reader.ReadF32();
            reader.Read(mat.albedo, sizeof(mat.albedo));
            mat.useAlphaClip = reader.ReadBool();
            mat.alphaClipThreshold = reader.ReadF32();
            mat.useAlphaBlend = reader.ReadBool();
            mat.doubleSided = reader.ReadBool();

            uint32 srvIndex = graphics->AllocateSRVIndex();
            mesh.LoadMaterial(mat, graphics, commandList, baseDirectory, srvIndex);
        }

        outData.meshes.push_back(std::move(mesh));
    }

    Logger::Info("[ModelCache] 静的モデルをキャッシュからロード: {} (メッシュ: {}個)", sourcePath, meshCount);
    return true;
}

// ========================================
// Skinned Model Cache
// ========================================
void ModelCache::SaveSkinnedModel(const std::string& sourcePath, const SkinnedModelData& data) {
    namespace fs = std::filesystem;

    std::string cachePath = GetCachePath(sourcePath, ".skcache");

    fs::path cacheDir(cacheDirectory_);
    if (!fs::exists(cacheDir)) {
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
        if (ec) return;
    }

    BinaryWriter writer(cachePath);
    if (!writer.IsOpen()) return;

    // Header
    writer.WriteU32(CACHE_MAGIC);
    writer.WriteU32(CACHE_VERSION_SKINNED);

    // Bounding box
    writer.WriteVector3(data.boundingBox.min);
    writer.WriteVector3(data.boundingBox.max);

    // Skeleton
    bool hasSkeleton = data.skeleton != nullptr;
    writer.WriteBool(hasSkeleton);
    if (hasSkeleton) {
        const auto& bones = data.skeleton->GetBones();
        writer.WriteU32(static_cast<uint32>(bones.size()));
        for (const auto& bone : bones) {
            writer.WriteString(bone.name);
            writer.WriteI32(bone.parentIndex);
            writer.WriteMatrix(bone.offsetMatrix);
            writer.WriteMatrix(bone.localBindPose);
        }
        writer.WriteMatrix(data.skeleton->GetGlobalInverseTransform());
    }

    // Animations
    writer.WriteU32(static_cast<uint32>(data.animations.size()));
    for (const auto& clip : data.animations) {
        writer.WriteString(clip->GetName());
        writer.WriteF32(clip->GetDuration());
        writer.WriteF32(clip->GetTicksPerSecond());

        const auto& boneAnims = clip->GetBoneAnimations();
        writer.WriteU32(static_cast<uint32>(boneAnims.size()));
        for (const auto& ba : boneAnims) {
            writer.WriteString(ba.boneName);

            // Position keys
            writer.WriteU32(static_cast<uint32>(ba.positionKeys.size()));
            for (const auto& k : ba.positionKeys) {
                writer.WriteF32(k.time);
                writer.WriteVector3(k.value);
            }

            // Rotation keys
            writer.WriteU32(static_cast<uint32>(ba.rotationKeys.size()));
            for (const auto& k : ba.rotationKeys) {
                writer.WriteF32(k.time);
                writer.WriteQuaternion(k.value);
            }

            // Scale keys
            writer.WriteU32(static_cast<uint32>(ba.scaleKeys.size()));
            for (const auto& k : ba.scaleKeys) {
                writer.WriteF32(k.time);
                writer.WriteVector3(k.value);
            }
        }
    }

    // Meshes
    writer.WriteU32(static_cast<uint32>(data.meshes.size()));
    for (const auto& mesh : data.meshes) {
        writer.WriteString(mesh.GetName());

        // Vertices (SkinnedVertex)
        const auto& verts = mesh.GetVertices();
        writer.WriteU32(static_cast<uint32>(verts.size()));
        if (!verts.empty()) {
            writer.Write(verts.data(), verts.size() * sizeof(SkinnedVertex));
        }

        // Indices
        const auto& inds = mesh.GetIndices();
        writer.WriteU32(static_cast<uint32>(inds.size()));
        if (!inds.empty()) {
            writer.Write(inds.data(), inds.size() * sizeof(uint32));
        }

        // Material data
        bool hasMaterial = mesh.HasMaterial();
        writer.WriteBool(hasMaterial);
        if (hasMaterial) {
            const auto& mat = mesh.GetMaterial()->GetData();
            writer.WriteString(mat.name);
            writer.Write(mat.ambient, sizeof(mat.ambient));
            writer.Write(mat.diffuse, sizeof(mat.diffuse));
            writer.Write(mat.specular, sizeof(mat.specular));
            writer.Write(mat.emissive, sizeof(mat.emissive));
            writer.WriteF32(mat.shininess);
            writer.WriteF32(mat.opacity);
            writer.WriteString(mat.diffuseTexturePath);
            writer.WriteF32(mat.metallic);
            writer.WriteF32(mat.roughness);
            writer.Write(mat.albedo, sizeof(mat.albedo));
            writer.WriteBool(mat.useAlphaClip);
            writer.WriteF32(mat.alphaClipThreshold);
            writer.WriteBool(mat.useAlphaBlend);
            writer.WriteBool(mat.doubleSided);
        }
    }

    Logger::Debug("[ModelCache] スキンモデルキャッシュ保存完了: {}", cachePath);
}

bool ModelCache::TryLoadSkinnedModel(const std::string& sourcePath,
                                      GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                      SkinnedModelData& outData) {
    std::string cachePath = GetCachePath(sourcePath, ".skcache");
    if (!IsCacheValid(sourcePath, cachePath)) return false;

    BinaryReader reader(cachePath);
    if (!reader.IsOpen()) return false;

    uint32 magic = reader.ReadU32();
    uint32 version = reader.ReadU32();
    if (magic != CACHE_MAGIC || version != CACHE_VERSION_SKINNED) return false;

    // Bounding box
    outData.boundingBox.min = reader.ReadVector3();
    outData.boundingBox.max = reader.ReadVector3();

    // Base directory for texture loading
    namespace fs = std::filesystem;
    std::string baseDirectory = fs::path(sourcePath).parent_path().string();

    // Skeleton
    bool hasSkeleton = reader.ReadBool();
    if (hasSkeleton) {
        outData.skeleton = std::make_shared<Skeleton>();
        uint32 boneCount = reader.ReadU32();
        for (uint32 i = 0; i < boneCount; ++i) {
            std::string name = reader.ReadString();
            int32 parentIndex = reader.ReadI32();
            Matrix4x4 offsetMatrix = reader.ReadMatrix();
            Matrix4x4 localBindPose = reader.ReadMatrix();
            outData.skeleton->AddBone(name, parentIndex, offsetMatrix, localBindPose);
        }
        Matrix4x4 globalInvTransform = reader.ReadMatrix();
        outData.skeleton->SetGlobalInverseTransform(globalInvTransform);
    }

    // Animations
    uint32 animCount = reader.ReadU32();
    outData.animations.reserve(animCount);
    for (uint32 a = 0; a < animCount; ++a) {
        auto clip = std::make_shared<AnimationClip>();
        clip->SetName(reader.ReadString());
        clip->SetDuration(reader.ReadF32());
        clip->SetTicksPerSecond(reader.ReadF32());

        uint32 boneAnimCount = reader.ReadU32();
        for (uint32 b = 0; b < boneAnimCount; ++b) {
            BoneAnimation ba;
            ba.boneName = reader.ReadString();

            uint32 posCount = reader.ReadU32();
            ba.positionKeys.resize(posCount);
            for (uint32 k = 0; k < posCount; ++k) {
                ba.positionKeys[k].time = reader.ReadF32();
                ba.positionKeys[k].value = reader.ReadVector3();
            }

            uint32 rotCount = reader.ReadU32();
            ba.rotationKeys.resize(rotCount);
            for (uint32 k = 0; k < rotCount; ++k) {
                ba.rotationKeys[k].time = reader.ReadF32();
                ba.rotationKeys[k].value = reader.ReadQuaternion();
            }

            uint32 scaleCount = reader.ReadU32();
            ba.scaleKeys.resize(scaleCount);
            for (uint32 k = 0; k < scaleCount; ++k) {
                ba.scaleKeys[k].time = reader.ReadF32();
                ba.scaleKeys[k].value = reader.ReadVector3();
            }

            clip->AddBoneAnimation(ba);
        }
        outData.animations.push_back(std::move(clip));
    }

    // Meshes
    uint32 meshCount = reader.ReadU32();
    outData.meshes.reserve(meshCount);
    for (uint32 i = 0; i < meshCount; ++i) {
        std::string meshName = reader.ReadString();

        uint32 vertCount = reader.ReadU32();
        std::vector<SkinnedVertex> vertices(vertCount);
        if (vertCount > 0) {
            reader.Read(vertices.data(), vertCount * sizeof(SkinnedVertex));
        }

        uint32 indexCount = reader.ReadU32();
        std::vector<uint32> indices(indexCount);
        if (indexCount > 0) {
            reader.Read(indices.data(), indexCount * sizeof(uint32));
        }

        SkinnedMesh mesh;
        mesh.Create(graphics->GetDevice(), commandList, vertices, indices, meshName);

        bool hasMaterial = reader.ReadBool();
        if (hasMaterial) {
            MaterialData mat;
            mat.name = reader.ReadString();
            reader.Read(mat.ambient, sizeof(mat.ambient));
            reader.Read(mat.diffuse, sizeof(mat.diffuse));
            reader.Read(mat.specular, sizeof(mat.specular));
            reader.Read(mat.emissive, sizeof(mat.emissive));
            mat.shininess = reader.ReadF32();
            mat.opacity = reader.ReadF32();
            mat.diffuseTexturePath = reader.ReadString();
            mat.metallic = reader.ReadF32();
            mat.roughness = reader.ReadF32();
            reader.Read(mat.albedo, sizeof(mat.albedo));
            mat.useAlphaClip = reader.ReadBool();
            mat.alphaClipThreshold = reader.ReadF32();
            mat.useAlphaBlend = reader.ReadBool();
            mat.doubleSided = reader.ReadBool();

            uint32 srvIndex = graphics->AllocateSRVIndex();
            mesh.LoadMaterial(mat, graphics, commandList, baseDirectory, srvIndex);
        }

        outData.meshes.push_back(std::move(mesh));
    }

    Logger::Info("[ModelCache] スキンモデルをキャッシュからロード: {} (メッシュ: {}個, アニメ: {}個)",
                 sourcePath, meshCount, animCount);
    return true;
}

} // namespace UnoEngine
