#include "Shader.h"
#include <stdexcept>

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
        if (errorBlob) {
            const char* errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
            throw std::runtime_error(std::string("Shader compilation failed: ") + errorMsg);
        }
        throw std::runtime_error("Shader compilation failed");
    }
}

} // namespace UnoEngine
