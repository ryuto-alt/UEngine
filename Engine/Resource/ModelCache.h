#pragma once

#include "../Core/Types.h"
#include "../Graphics/Mesh.h"
#include "../Graphics/SkinnedMesh.h"
#include "../Graphics/SkinnedVertex.h"
#include "../Graphics/Material.h"
#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Math/Quaternion.h"
#include "StaticModelImporter.h"
#include "SkinnedModelImporter.h"
#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace UnoEngine {

class GraphicsDevice;

// モデルのバイナリキャッシュシステム
// Assimpパースをスキップし、処理済みの頂点/インデックス/マテリアル/スケルトン/アニメーションを
// バイナリファイルから直接読み込む
class ModelCache {
public:
    static void SetEnabled(bool enabled) { enabled_ = enabled; }
    static bool IsEnabled() { return enabled_; }
    static void SetCacheDirectory(const std::string& dir) { cacheDirectory_ = dir; }

    // キャッシュからStaticModelを読み込む（成功時true）
    static bool TryLoadStaticModel(const std::string& sourcePath,
                                    GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                    StaticModelData& outData);

    // StaticModelをキャッシュに保存
    static void SaveStaticModel(const std::string& sourcePath,
                                 const StaticModelData& data);

    // キャッシュからSkinnedModelを読み込む
    static bool TryLoadSkinnedModel(const std::string& sourcePath,
                                     GraphicsDevice* graphics, ID3D12GraphicsCommandList* commandList,
                                     SkinnedModelData& outData);

    // SkinnedModelをキャッシュに保存
    static void SaveSkinnedModel(const std::string& sourcePath,
                                  const SkinnedModelData& data);

    // キャッシュが有効か（ソースより新しいか）
    static bool IsCacheValid(const std::string& sourcePath, const std::string& cachePath);

private:
    // バイナリ書き込みヘルパー
    class BinaryWriter {
    public:
        explicit BinaryWriter(const std::string& path);
        ~BinaryWriter();
        bool IsOpen() const;
        void Write(const void* data, size_t size);
        void WriteU32(uint32 val);
        void WriteI32(int32 val);
        void WriteF32(float val);
        void WriteString(const std::string& str);
        void WriteBool(bool val);
        void WriteVector3(const Vector3& v);
        void WriteMatrix(const Matrix4x4& m);
        void WriteQuaternion(const Quaternion& q);
    private:
        std::ofstream stream_;
    };

    // バイナリ読み込みヘルパー
    class BinaryReader {
    public:
        explicit BinaryReader(const std::string& path);
        ~BinaryReader();
        bool IsOpen() const;
        void Read(void* data, size_t size);
        uint32 ReadU32();
        int32 ReadI32();
        float ReadF32();
        std::string ReadString();
        bool ReadBool();
        Vector3 ReadVector3();
        Matrix4x4 ReadMatrix();
        Quaternion ReadQuaternion();
    private:
        std::ifstream stream_;
    };

    static std::string GetCachePath(const std::string& sourcePath, const std::string& suffix);

    static bool enabled_;
    static std::string cacheDirectory_;

    static constexpr uint32 CACHE_MAGIC = 0x554D4348;  // "UMCH"
    static constexpr uint32 CACHE_VERSION_STATIC = 1;
    static constexpr uint32 CACHE_VERSION_SKINNED = 1;
};

} // namespace UnoEngine
