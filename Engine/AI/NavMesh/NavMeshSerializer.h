#pragma once

#include "NavMeshTypes.h"
#include <filesystem>
#include <string>

namespace UnoEngine
{

// NavMeshバイナリシリアライザ
class NavMeshSerializer
{
public:
    // ファイル拡張子
    static constexpr const char* FILE_EXTENSION = ".navmesh";
    
    // バイナリ形式でNavMeshを保存
    static bool Save(const NavMeshData& navMesh, const std::filesystem::path& filePath);
    
    // バイナリ形式からNavMeshを読み込み
    static std::unique_ptr<NavMeshData> Load(const std::filesystem::path& filePath);
    
    // ファイルが有効なNavMeshファイルか確認
    static bool IsValidNavMeshFile(const std::filesystem::path& filePath);
    
private:
    // ファイルヘッダー
    struct FileHeader
    {
        char magic[4] = {'N', 'A', 'V', 'M'};  // マジックナンバー
        uint32_t version = 1;                   // フォーマットバージョン
        uint32_t vertexCount = 0;
        uint32_t polygonCount = 0;
    };
    
    static constexpr uint32_t CURRENT_VERSION = 1;
};

} // namespace UnoEngine
