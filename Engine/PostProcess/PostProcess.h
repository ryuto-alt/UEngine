#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "../Graphics/D3D12Common.h"
#include "PostProcessType.h"

namespace UnoEngine {

class GraphicsDevice;
class RenderTexture;

// ポストプロセス基底クラス
class PostProcess : public NonCopyable {
public:
    PostProcess() = default;
    virtual ~PostProcess() = default;

    virtual void Initialize(GraphicsDevice* graphics) = 0;
    virtual void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) = 0;
    virtual PostProcessType GetType() const = 0;
    virtual const char* GetName() const = 0;

    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

protected:
    bool m_enabled = true;
};

} // namespace UnoEngine
