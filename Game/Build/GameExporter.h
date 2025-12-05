#pragma once

#include <string>
#include <vector>
#include <functional>

namespace UnoEngine {

struct ExportSettings {
    std::wstring outputPath;
    std::wstring gameName = L"Game";
    bool copyShaders = true;
    bool copyScenes = true;
    bool copyModels = true;
    bool copyTextures = true;
    bool copyAudio = true;
};

struct ExportProgress {
    int currentStep = 0;
    int totalSteps = 0;
    std::string currentTask;
    float progress = 0.0f;
};

using ExportProgressCallback = std::function<void(const ExportProgress&)>;

class GameExporter {
public:
    GameExporter() = default;
    ~GameExporter() = default;

    // エクスポート実行（ビルド含む）
    bool Export(const ExportSettings& settings, ExportProgressCallback progressCallback = nullptr);

    // MSBuildでUnoGame.vcxprojをビルド
    bool BuildGame(ExportProgressCallback progressCallback = nullptr);

    // 最後のエラーメッセージ取得
    const std::string& GetLastError() const { return m_lastError; }

    // ビルドログ取得
    const std::string& GetBuildLog() const { return m_buildLog; }

    // フォルダ選択ダイアログを表示
    static std::wstring ShowFolderDialog(void* hwnd, const wchar_t* title);

    // MSBuild.exeのパスを検索
    static std::wstring FindMSBuild();

private:
    bool CopyGameExecutable(const std::wstring& outputPath, const std::wstring& gameName);
    bool CopyShadersFolder(const std::wstring& outputPath);
    bool CopyAssetsFolder(const std::wstring& outputPath, const ExportSettings& settings);
    bool CopyRuntimeDLLs(const std::wstring& outputPath);

    void ReportProgress(int step, int total, const std::string& task);

    std::string m_lastError;
    std::string m_buildLog;
    ExportProgressCallback m_progressCallback;
};

} // namespace UnoEngine
