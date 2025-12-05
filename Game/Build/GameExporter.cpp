#include "pch.h"
#include "GameExporter.h"
#include <filesystem>
#include <Windows.h>
#include <ShlObj.h>
#include <format>

namespace fs = std::filesystem;

namespace UnoEngine {

std::wstring GameExporter::FindMSBuild() {
    // VS2022/2019の一般的なMSBuildパス
    std::vector<std::wstring> candidates = {
        L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
        L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
        L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
        L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
    };

    for (const auto& path : candidates) {
        if (fs::exists(path)) {
            return path;
        }
    }

    // vswhere.exeで検索
    std::wstring vswherePath = L"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (fs::exists(vswherePath)) {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        std::wstring cmd = L"\"" + vswherePath + L"\" -latest -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\MSBuild.exe";

        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        HANDLE hReadPipe, hWritePipe;
        CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hWritePipe);

            char buffer[4096] = {};
            DWORD bytesRead;
            ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);

            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);

            // 改行を除去して結果を返す
            std::string result = buffer;
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }

            if (!result.empty() && fs::exists(result)) {
                return std::wstring(result.begin(), result.end());
            }
        } else {
            CloseHandle(hWritePipe);
            CloseHandle(hReadPipe);
        }
    }

    return L"";
}

bool GameExporter::BuildGame(ExportProgressCallback progressCallback) {
    m_progressCallback = progressCallback;
    m_lastError.clear();
    m_buildLog.clear();

    ReportProgress(0, 3, "MSBuildを検索中...");

    // MSBuildを検索
    std::wstring msbuildPath = FindMSBuild();
    if (msbuildPath.empty()) {
        m_lastError = "MSBuild.exe が見つかりません。Visual Studio がインストールされていることを確認してください。";
        return false;
    }

    ReportProgress(1, 3, "UnoGame をビルド中...");

    // プロジェクトファイルのパス
    fs::path projectPath = fs::current_path() / "UnoGame.vcxproj";
    if (!fs::exists(projectPath)) {
        m_lastError = "UnoGame.vcxproj が見つかりません。";
        return false;
    }

    // MSBuildコマンドを構築
    std::wstring cmd = L"\"" + msbuildPath + L"\" \"" + projectPath.wstring() +
                       L"\" /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal";

    // プロセスを起動してビルド実行
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        m_lastError = "パイプの作成に失敗しました。";
        return false;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        m_lastError = "MSBuild プロセスの起動に失敗しました。";
        return false;
    }

    CloseHandle(hWritePipe);

    // ビルド出力を読み取る
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        m_buildLog += buffer;
    }

    // プロセス終了を待つ
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    ReportProgress(2, 3, "ビルド結果を確認中...");

    if (exitCode != 0) {
        m_lastError = "ビルドに失敗しました。\n\n" + m_buildLog;
        return false;
    }

    ReportProgress(3, 3, "ビルド完了");
    return true;
}

bool GameExporter::Export(const ExportSettings& settings, ExportProgressCallback progressCallback) {
    m_progressCallback = progressCallback;
    m_lastError.clear();

    if (settings.outputPath.empty()) {
        m_lastError = "出力パスが指定されていません";
        return false;
    }

    int totalSteps = 5; // build + exe + shaders + assets + dlls
    int currentStep = 0;

    // 1. まずビルドを実行
    ReportProgress(++currentStep, totalSteps, "ゲームをビルド中...");
    if (!BuildGame(nullptr)) {
        return false;
    }

    // 出力フォルダを作成
    try {
        fs::create_directories(settings.outputPath);
    } catch (const std::exception& e) {
        m_lastError = std::format("出力フォルダの作成に失敗: {}", e.what());
        return false;
    }

    // 2. Game.exe をコピー
    ReportProgress(++currentStep, totalSteps, "実行ファイルをコピー中...");
    if (!CopyGameExecutable(settings.outputPath, settings.gameName)) {
        return false;
    }

    // 3. Shaders フォルダをコピー
    ReportProgress(++currentStep, totalSteps, "シェーダーをコピー中...");
    if (settings.copyShaders) {
        if (!CopyShadersFolder(settings.outputPath)) {
            return false;
        }
    }

    // 4. assets フォルダをコピー
    ReportProgress(++currentStep, totalSteps, "アセットをコピー中...");
    if (!CopyAssetsFolder(settings.outputPath, settings)) {
        return false;
    }

    // 5. 必要なDLLをコピー
    ReportProgress(++currentStep, totalSteps, "ランタイムDLLをコピー中...");
    if (!CopyRuntimeDLLs(settings.outputPath)) {
        return false;
    }

    ReportProgress(totalSteps, totalSteps, "エクスポート完了");
    return true;
}

bool GameExporter::CopyGameExecutable(const std::wstring& outputPath, const std::wstring& gameName) {
    // UnoGame.exe のパス (ビルド後)
    fs::path sourcePath = fs::current_path() / "build" / "Release" / "UnoGame.exe";

    if (!fs::exists(sourcePath)) {
        m_lastError = "UnoGame.exe が見つかりません。ビルドが正常に完了していることを確認してください。\n"
                      "検索パス: " + sourcePath.string();
        return false;
    }

    // 出力先パス
    fs::path destPath = fs::path(outputPath) / (gameName + L".exe");

    try {
        fs::copy_file(sourcePath, destPath, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        m_lastError = std::format("実行ファイルのコピーに失敗: {}", e.what());
        return false;
    }

    return true;
}

bool GameExporter::CopyShadersFolder(const std::wstring& outputPath) {
    fs::path shadersSource = fs::current_path() / "Shaders";
    fs::path shadersDest = fs::path(outputPath) / "Shaders";

    if (!fs::exists(shadersSource)) {
        m_lastError = "Shaders フォルダが見つかりません";
        return false;
    }

    try {
        fs::copy(shadersSource, shadersDest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        m_lastError = std::format("シェーダーのコピーに失敗: {}", e.what());
        return false;
    }

    return true;
}

bool GameExporter::CopyRuntimeDLLs(const std::wstring& outputPath) {
    // assimp DLLをコピー
    fs::path assimpDll = fs::current_path() / "external" / "assimp" / "bin" / "Release" / "assimp-vc145-mt.dll";

    if (fs::exists(assimpDll)) {
        try {
            fs::copy_file(assimpDll, fs::path(outputPath) / assimpDll.filename(),
                         fs::copy_options::overwrite_existing);
        } catch (const std::exception&) {
            // DLLコピー失敗は警告のみ
        }
    }

    return true;
}

bool GameExporter::CopyAssetsFolder(const std::wstring& outputPath, const ExportSettings& settings) {
    fs::path assetsSource = fs::current_path() / "assets";
    fs::path assetsDest = fs::path(outputPath) / "assets";

    if (!fs::exists(assetsSource)) {
        m_lastError = "assets フォルダが見つかりません";
        return false;
    }

    try {
        // assets フォルダを再帰的にコピー
        fs::create_directories(assetsDest);

        for (const auto& entry : fs::recursive_directory_iterator(assetsSource)) {
            const auto& sourcePath = entry.path();
            auto relativePath = fs::relative(sourcePath, assetsSource);
            auto destPath = assetsDest / relativePath;

            // フィルタリング
            std::string ext = sourcePath.extension().string();
            std::string parentDir = sourcePath.parent_path().filename().string();

            // 設定に応じてスキップ
            if (!settings.copyShaders && parentDir == "shaders") continue;
            if (!settings.copyScenes && parentDir == "scenes") continue;
            if (!settings.copyModels && (parentDir == "models" || ext == ".fbx" || ext == ".obj" || ext == ".gltf")) continue;
            if (!settings.copyTextures && (ext == ".png" || ext == ".jpg" || ext == ".dds" || ext == ".tga")) continue;
            if (!settings.copyAudio && (ext == ".wav" || ext == ".mp3" || ext == ".ogg")) continue;

            if (entry.is_directory()) {
                fs::create_directories(destPath);
            } else {
                // 親ディレクトリを確保
                fs::create_directories(destPath.parent_path());
                fs::copy_file(sourcePath, destPath, fs::copy_options::overwrite_existing);
            }
        }
    } catch (const std::exception& e) {
        m_lastError = std::format("アセットのコピーに失敗: {}", e.what());
        return false;
    }

    return true;
}

void GameExporter::ReportProgress(int step, int total, const std::string& task) {
    if (m_progressCallback) {
        ExportProgress progress;
        progress.currentStep = step;
        progress.totalSteps = total;
        progress.currentTask = task;
        progress.progress = static_cast<float>(step) / static_cast<float>(total);
        m_progressCallback(progress);
    }
}

std::wstring GameExporter::ShowFolderDialog(void* hwnd, const wchar_t* title) {
    std::wstring result;

    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pfd->GetOptions(&dwOptions);
        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        pfd->SetTitle(title);

        hr = pfd->Show(static_cast<HWND>(hwnd));
        if (SUCCEEDED(hr)) {
            IShellItem* psi = nullptr;
            hr = pfd->GetResult(&psi);
            if (SUCCEEDED(hr)) {
                PWSTR pszPath = nullptr;
                hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                if (SUCCEEDED(hr)) {
                    result = pszPath;
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    return result;
}

} // namespace UnoEngine
