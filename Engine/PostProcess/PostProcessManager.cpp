#include "pch.h"
#include "PostProcessManager.h"
#include "../Graphics/GraphicsDevice.h"

namespace UnoEngine {

void PostProcessManager::Initialize(GraphicsDevice* graphics, uint32 width, uint32 height) {
    m_grayscale = std::make_unique<GrayscalePostProcess>();
    m_grayscale->Initialize(graphics);

    m_vignette = std::make_unique<VignettePostProcess>();
    m_vignette->Initialize(graphics);

    m_fisheye = std::make_unique<FisheyePostProcess>();
    m_fisheye->Initialize(graphics);
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
        case PostProcessType::Vignette:
            if (m_vignette && m_vignette->IsEnabled()) {
                m_vignette->Apply(graphics, source, destination);
            }
            break;
        case PostProcessType::Fisheye:
            if (m_fisheye && m_fisheye->IsEnabled()) {
                m_fisheye->Apply(graphics, source, destination);
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
