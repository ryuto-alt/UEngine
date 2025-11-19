#pragma once

#include "../Core/Camera.h"
#include "../Core/Types.h"
#include <string>

namespace UnoEngine {

/// レンダリングビューの設定
/// OnRenderメソッドで使用され、どのカメラでどのレイヤーをレンダリングするかを指定します
struct RenderView {
    /// レンダリングに使用するカメラ
    /// このカメラの視点と投影設定でシーンがレンダリングされます
    Camera* camera = nullptr;
    
    /// レンダリング対象のレイヤーマスク
    /// ビット演算で複数レイヤーを指定可能（例: 0x00000001 = Layer 0のみ、0xFFFFFFFF = 全レイヤー）
    uint32 layerMask = 0xFFFFFFFF;
    
    /// ビューの識別名
    /// デバッグやプロファイリング時にビューを区別するために使用されます
    std::string viewName = "Main";
};

} // namespace UnoEngine
