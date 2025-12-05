#pragma once

#include "PostProcess.h"
#include "PostProcessPipeline.h"

namespace UnoEngine {

class GrayscalePostProcess : public PostProcess {
public:
    GrayscalePostProcess() = default;
    ~GrayscalePostProcess() override = default;

    void Initialize(GraphicsDevice* graphics) override;
    void Apply(GraphicsDevice* graphics, RenderTexture* source, RenderTexture* destination) override;
    PostProcessType GetType() const override { return PostProcessType::Grayscale; }
    const char* GetName() const override { return "Grayscale"; }

private:
    PostProcessPipeline m_pipeline;
};

} // namespace UnoEngine
