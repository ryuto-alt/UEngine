#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace UnoEngine {

class GraphicsDevice;
class Window;

class ImGuiManager : public NonCopyable {
public:
    ImGuiManager() = default;
    ~ImGuiManager();

    void Initialize(GraphicsDevice* graphics, Window* window, uint32 srvDescriptorIndex);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Render(ID3D12GraphicsCommandList* commandList);

    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

private:
    GraphicsDevice* graphics_ = nullptr;
    uint32 srvDescriptorIndex_ = 0;
    bool initialized_ = false;
};

} // namespace UnoEngine
