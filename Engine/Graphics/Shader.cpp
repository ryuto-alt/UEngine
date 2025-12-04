#include "pch.h"
#include "Shader.h"
#include <stdexcept>
#include <Windows.h>

namespace UnoEngine {

void Shader::CompileFromFile(const std::wstring& filepath, ShaderStage stage, const std::string& entryPoint) {
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    const char* target = nullptr;
    switch (stage) {
    case ShaderStage::Vertex:
        target = "vs_5_1";
        break;
    case ShaderStage::Pixel:
        target = "ps_5_1";
        break;
    case ShaderStage::Compute:
        target = "cs_5_1";
        break;
    default:
        throw std::runtime_error("Unsupported shader stage");
    }

    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompileFromFile(
        filepath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(),
        target,
        compileFlags,
        0,
        &bytecode_,
        &errorBlob
    );

    if (FAILED(hr)) {
        std::string errorMessage = "Shader compilation failed";

        // ファイルパスを追加 (WideCharToMultiByte使用)
        int size = WideCharToMultiByte(CP_UTF8, 0, filepath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string pathStr(size - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, filepath.c_str(), -1, &pathStr[0], size, nullptr, nullptr);
            errorMessage += "\nFile: " + pathStr;
        }

        if (errorBlob) {
            const char* errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
            errorMessage += "\nError: ";
            errorMessage += errorMsg;
        } else {
            errorMessage += "\nNo error details (file may not exist)";
        }

        // デバッグ出力
        OutputDebugStringA(errorMessage.c_str());

        // メッセージボックスで詳細表示
        MessageBoxA(nullptr, errorMessage.c_str(), "Shader Error", MB_OK | MB_ICONERROR);

        throw std::runtime_error(errorMessage);
    }
}

} // namespace UnoEngine
