#pragma once

#include "../Core/Types.h"
#include "../Core/NonCopyable.h"
#include "PostProcessType.h"
#include "GrayscalePostProcess.h"
#include "VignettePostProcess.h"
#include "FisheyePostProcess.h"
#include "../Graphics/RenderTexture.h"
#include <memory>
#include <vector>

namespace UnoEngine {

class GraphicsDevice;

// ポストプロセスマネージャー
class PostProcessManager : public NonCopyable {
public:
    PostProcessManager() = default;
    ~PostProcessManager() = default;

    void Initialize(GraphicsDevice* graphics, uint32 width, uint32 height);
    void Resize(GraphicsDevice* graphics, uint32 width, uint32 height);

    // エフェクトチェーン管理
    void AddEffect(PostProcessType type);
    void RemoveEffect(PostProcessType type);
    void ClearEffects();
    void SetEffectEnabled(PostProcessType type, bool enabled);
    bool IsEffectEnabled(PostProcessType type) const;
    bool IsEffectInChain(PostProcessType type) const;
    const std::vector<PostProcessType>& GetEffectChain() const { return m_effectChain; }
    void SetEffectChain(const std::vector<PostProcessType>& chain) { m_effectChain = chain; }

    // 旧API互換（単一エフェクト選択）
    void SetActiveEffect(PostProcessType type);
    PostProcessType GetActiveEffect() const { return m_effectChain.empty() ? PostProcessType::None : m_effectChain[0]; }

    // ソーステクスチャにポストプロセスを適用し、結果をdestinationに出力
    void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination);

    // 利用可能なエフェクト一覧取得（UI用）
    static constexpr int GetEffectCount() { return static_cast<int>(PostProcessType::Count); }
    static const char* GetEffectName(int index);

    // パラメータアクセス
    GrayscalePostProcess* GetGrayscale() { return m_grayscale.get(); }
    VignettePostProcess* GetVignette() { return m_vignette.get(); }
    FisheyePostProcess* GetFisheye() { return m_fisheye.get(); }

private:
    PostProcess* GetEffectByType(PostProcessType type);
    void CreateIntermediateBuffers(GraphicsDevice* graphics, uint32 width, uint32 height);

    std::vector<PostProcessType> m_effectChain;
    std::unique_ptr<GrayscalePostProcess> m_grayscale;
    std::unique_ptr<VignettePostProcess> m_vignette;
    std::unique_ptr<FisheyePostProcess> m_fisheye;

    // Ping-Pong バッファ（チェーン処理用）
    std::unique_ptr<RenderTexture> m_intermediateA;
    std::unique_ptr<RenderTexture> m_intermediateB;
    uint32 m_width = 0;
    uint32 m_height = 0;
};

} // namespace UnoEngine
