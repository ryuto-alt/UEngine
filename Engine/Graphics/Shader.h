#pragma once

#include "D3D12Common.h"
#include <string>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace UnoEngine {

// シェーダーステージ
enum class ShaderStage {
    Vertex,
    Pixel,
    Compute
};

// シェーダークラス
class Shader {
public:
    Shader() = default;
    ~Shader() = default;

    // ファイルからコンパイル
    void CompileFromFile(const std::wstring& filepath, ShaderStage stage, const std::string& entryPoint = "main");

    // バイトコード取得
    ID3DBlob* GetBytecode() const { return bytecode_.Get(); }
    D3D12_SHADER_BYTECODE GetBytecodeDesc() const {
        return { bytecode_->GetBufferPointer(), bytecode_->GetBufferSize() };
    }

private:
    ComPtr<ID3DBlob> bytecode_;
};

} // namespace UnoEngine
