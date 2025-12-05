#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "PostProcessType.h"
#include "GrayscalePostProcess.h"
#include "VignettePostProcess.h"
#include "FisheyePostProcess.h"
#include "../Graphics/RenderTexture.h"
#include <memory>

namespace UnoEngine {

class GraphicsDevice;

// ポストプロセスマネージャー
class PostProcessManager : public NonCopyable {
public:
    PostProcessManager() = default;
    ~PostProcessManager() = default;

    void Initialize(GraphicsDevice* graphics, uint32 width, uint32 height);
    void Resize(GraphicsDevice* graphics, uint32 width, uint32 height);

    void SetActiveEffect(PostProcessType type);
    PostProcessType GetActiveEffect() const { return m_activeEffect; }

    // ソーステクスチャにポストプロセスを適用し、結果をdestinationに出力
    void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination);

    // 利用可能なエフェクト一覧取得（UI用）
    static constexpr int GetEffectCount() { return static_cast<int>(PostProcessType::Count); }
    static const char* GetEffectName(int index);

    // パラメータアクセス
    VignettePostProcess* GetVignette() { return m_vignette.get(); }
    FisheyePostProcess* GetFisheye() { return m_fisheye.get(); }

private:
    PostProcessType m_activeEffect = PostProcessType::None;
    std::unique_ptr<GrayscalePostProcess> m_grayscale;
    std::unique_ptr<VignettePostProcess> m_vignette;
    std::unique_ptr<FisheyePostProcess> m_fisheye;
};

} // namespace UnoEngine
