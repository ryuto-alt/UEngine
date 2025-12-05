#include "pch.h"
#include "PostProcessManager.h"
#include "../Graphics/GraphicsDevice.h"
#include <algorithm>

namespace UnoEngine {

// SRVインデックス（中間バッファ用）
constexpr uint32 kIntermediateASrvIndex = 200;
constexpr uint32 kIntermediateBSrvIndex = 201;

void PostProcessManager::Initialize(GraphicsDevice* graphics, uint32 width, uint32 height) {
    m_grayscale = std::make_unique<GrayscalePostProcess>();
    m_grayscale->Initialize(graphics);

    m_vignette = std::make_unique<VignettePostProcess>();
    m_vignette->Initialize(graphics);

    m_fisheye = std::make_unique<FisheyePostProcess>();
    m_fisheye->Initialize(graphics);

    m_width = width;
    m_height = height;
}

void PostProcessManager::Resize(GraphicsDevice* graphics, uint32 width, uint32 height) {
    if (width == 0 || height == 0) return;
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    if (m_intermediateA) {
        m_intermediateA->Resize(graphics, width, height);
    }
    if (m_intermediateB) {
        m_intermediateB->Resize(graphics, width, height);
    }
}

void PostProcessManager::CreateIntermediateBuffers(GraphicsDevice* graphics, uint32 width, uint32 height) {
    if (!m_intermediateA) {
        m_intermediateA = std::make_unique<RenderTexture>();
        m_intermediateA->Create(graphics, width, height, kIntermediateASrvIndex);
    }
    if (!m_intermediateB) {
        m_intermediateB = std::make_unique<RenderTexture>();
        m_intermediateB->Create(graphics, width, height, kIntermediateBSrvIndex);
    }
}

void PostProcessManager::AddEffect(PostProcessType type) {
    if (type == PostProcessType::None || type == PostProcessType::Count) return;
    if (IsEffectInChain(type)) return;

    m_effectChain.push_back(type);

    auto* effect = GetEffectByType(type);
    if (effect) {
        effect->SetEnabled(true);
    }
}

void PostProcessManager::RemoveEffect(PostProcessType type) {
    auto it = std::find(m_effectChain.begin(), m_effectChain.end(), type);
    if (it != m_effectChain.end()) {
        m_effectChain.erase(it);
    }
}

void PostProcessManager::ClearEffects() {
    m_effectChain.clear();
}

void PostProcessManager::SetEffectEnabled(PostProcessType type, bool enabled) {
    auto* effect = GetEffectByType(type);
    if (effect) {
        effect->SetEnabled(enabled);
    }
}

bool PostProcessManager::IsEffectEnabled(PostProcessType type) const {
    auto* self = const_cast<PostProcessManager*>(this);
    auto* effect = self->GetEffectByType(type);
    return effect && effect->IsEnabled();
}

bool PostProcessManager::IsEffectInChain(PostProcessType type) const {
    return std::find(m_effectChain.begin(), m_effectChain.end(), type) != m_effectChain.end();
}

void PostProcessManager::SetActiveEffect(PostProcessType type) {
    ClearEffects();
    if (type != PostProcessType::None) {
        AddEffect(type);
    }
}

PostProcess* PostProcessManager::GetEffectByType(PostProcessType type) {
    switch (type) {
        case PostProcessType::Grayscale: return m_grayscale.get();
        case PostProcessType::Vignette: return m_vignette.get();
        case PostProcessType::Fisheye: return m_fisheye.get();
        default: return nullptr;
    }
}

void PostProcessManager::Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) {
    // 有効なエフェクトをフィルタリング
    std::vector<PostProcessType> activeEffects;
    for (auto type : m_effectChain) {
        auto* effect = GetEffectByType(type);
        if (effect && effect->IsEnabled()) {
            activeEffects.push_back(type);
        }
    }

    if (activeEffects.empty()) {
        return;
    }

    // 単一エフェクトの場合は直接適用
    if (activeEffects.size() == 1) {
        auto* effect = GetEffectByType(activeEffects[0]);
        effect->Apply(graphics, source, destination);
        return;
    }

    // 複数エフェクトの場合はPing-Pongバッファを使用
    CreateIntermediateBuffers(graphics, m_width, m_height);

    RenderTexture* input = source;
    RenderTexture* output = m_intermediateA.get();
    bool useBufferA = true;

    for (size_t i = 0; i < activeEffects.size(); ++i) {
        bool isLast = (i == activeEffects.size() - 1);

        if (isLast) {
            output = destination;
        }

        auto* effect = GetEffectByType(activeEffects[i]);
        effect->Apply(graphics, input, output);

        if (!isLast) {
            input = output;
            output = useBufferA ? m_intermediateB.get() : m_intermediateA.get();
            useBufferA = !useBufferA;
        }
    }
}

const char* PostProcessManager::GetEffectName(int index) {
    return PostProcessTypeToString(static_cast<PostProcessType>(index));
}

} // namespace UnoEngine
