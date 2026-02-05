#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>

class DirectXCommon;
class SrvManager;

class PostProcess {
public:
    enum class EffectType {
        Horror,
        TitleNoise,
        PSXRetro
    };

    struct HorrorParams {
        float time;
        float noiseIntensity;
        float distortionAmount;
        float bloodAmount;
        float vignetteIntensity;
        float fisheyeStrength;
        float fisheyeRadius;
        float padding;
    };

    struct TitleNoiseParams {
        float time;
        float grainIntensity;
        float scanlineIntensity;
        float scanlineCount;
        float glitchIntensity;
        float glitchFrequency;
        float chromaticStrength;
        float vignetteIntensity;
    };

    struct PSXParams {
        float screenWidth;
        float screenHeight;
        float targetWidth;
        float targetHeight;
        int colorDepth;
        int enableDithering;
        float padding0;
        float padding1;
    };

    PostProcess() = default;
    ~PostProcess();

    void Finalize();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, EffectType type = EffectType::Horror);
    void PreDraw();
    void PostDraw();
    void PostDrawTo(PostProcess* nextEffect);

    // Horror
    void SetHorrorParams(float time, float noise, float distortion, float blood, float vignette = 0.0f);
    void SetFisheyeStrength(float strength);
    void SetFisheyeRadius(float radius);

    // TitleNoise
    void SetTitleNoiseParams(float time, float grain, float scanline, float scanlineCount,
                             float glitch, float glitchFreq, float chromatic, float vignette);

    // PSX
    void SetPSXParams(float screenW, float screenH, float targetW, float targetH,
                      int colorDepth, bool dithering);

    void ResizeRenderTarget();

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }
    ID3D12Resource* GetRenderTarget() const { return renderTargetResource_.Get(); }
    EffectType GetEffectType() const { return effectType_; }

private:
    void CreateRenderTarget();
    void CreatePipeline();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    EffectType effectType_ = EffectType::Horror;

    // Render Target
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargetResource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGPUHandle_{};
    uint32_t srvIndex_ = 0;
    bool srvAllocated_ = false;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;

    // Pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // Constant Buffer (32 bytes for all effect types)
    Microsoft::WRL::ComPtr<ID3D12Resource> paramsResource_;
    void* paramsData_ = nullptr;

    HorrorParams currentHorrorParams_{};
    TitleNoiseParams currentNoiseParams_{};
    PSXParams currentPSXParams_{};

    uint32_t backBufferIndex_ = 0;
};
