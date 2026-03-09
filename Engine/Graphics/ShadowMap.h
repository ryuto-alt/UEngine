#pragma once

#include "D3D12Common.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"

namespace UnoEngine {

class GraphicsDevice;

class ShadowMap {
public:
    ShadowMap() = default;
    ~ShadowMap() = default;

    void Create(GraphicsDevice* graphics, uint32_t resolution = 2048);

    void BeginShadowPass(ID3D12GraphicsCommandList* cmdList);
    void EndShadowPass(ID3D12GraphicsCommandList* cmdList);
    void RestoreForNextFrame(ID3D12GraphicsCommandList* cmdList);

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return srvHandle_; }

    uint32_t GetResolution() const { return resolution_; }

    // Compute orthographic light VP from directional light direction + scene bounds
    static Matrix4x4 ComputeLightViewProj(const Vector3& lightDir,
                                          const Vector3& sceneCenter,
                                          float sceneRadius,
                                          uint32_t resolution = 2048);

    // Compute perspective VP for a spot light
    static Matrix4x4 ComputeSpotLightViewProj(const Vector3& position,
                                               const Vector3& direction,
                                               float spotAngleRad,
                                               float range);

private:
    ComPtr<ID3D12Resource>      depthBuffer_;
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle_{};
    uint32_t resolution_ = 2048;
    bool created_        = false;
};

} // namespace UnoEngine
