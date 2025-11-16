#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace UnoEngine {

// ComPtr alias for DirectX objects
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// HRESULT チェック用ヘルパー
inline void ThrowIfFailed(HRESULT hr, const char* msg = "DirectX operation failed") {
    if (FAILED(hr)) {
        throw std::runtime_error(msg);
    }
}

// 定数
constexpr uint32_t BACK_BUFFER_COUNT = 2;

} // namespace UnoEngine
