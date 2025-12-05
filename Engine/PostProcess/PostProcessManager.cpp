#include "pch.h"
#include "PostProcessManager.h"
#include "../Graphics/GraphicsDevice.h"

namespace UnoEngine {

void PostProcessManager::Initialize(GraphicsDevice* graphics, uint32 width, uint32 height) {
    // グレースケールエフェクト初期化
    m_grayscale = std::make_unique<GrayscalePostProcess>();
    m_grayscale->Initialize(graphics);
}

void PostProcessManager::Resize(GraphicsDevice* graphics, uint32 width, uint32 height) {
    // 現在は中間バッファを使用しないため何もしない
}

void PostProcessManager::SetActiveEffect(PostProcessType type) {
    m_activeEffect = type;
}

void PostProcessManager::Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) {
    if (m_activeEffect == PostProcessType::None) {
        return;
    }

    switch (m_activeEffect) {
        case PostProcessType::Grayscale:
            if (m_grayscale && m_grayscale->IsEnabled()) {
                m_grayscale->Apply(graphics, source, destination);
            }
            break;
        default:
            break;
    }
}

const char* PostProcessManager::GetEffectName(int index) {
    return PostProcessTypeToString(static_cast<PostProcessType>(index));
}

} // namespace UnoEngine
