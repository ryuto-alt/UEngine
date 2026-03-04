#include "pch.h"
#include "ShadowMap.h"
#include "GraphicsDevice.h"
#include "../Core/Logger.h"
#include "../Math/MathCommon.h"
#include <cmath>

namespace UnoEngine {

void ShadowMap::Create(GraphicsDevice* graphics, uint32_t resolution) {
    resolution_ = resolution;
    auto* device = graphics->GetDevice();

    // Depth texture (D32_FLOAT for DSV, R32_FLOAT for SRV)
    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width              = resolution;
    desc.Height             = resolution;
    desc.DepthOrArraySize   = 1;
    desc.MipLevels          = 1;
    desc.Format             = DXGI_FORMAT_R32_TYPELESS; // typed as D32 for DSV, R32 for SRV
    desc.SampleDesc.Count   = 1;
    desc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_CLEAR_VALUE clearVal = {};
    clearVal.Format               = DXGI_FORMAT_D32_FLOAT;
    clearVal.DepthStencil.Depth   = 1.0f;
    clearVal.DepthStencil.Stencil = 0;

    ThrowIfFailed(
        device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE,
            &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearVal, IID_PPV_ARGS(&depthBuffer_)),
        "Failed to create shadow map depth buffer"
    );
    depthBuffer_->SetName(L"ShadowMap_DepthBuffer");

    // DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(
        device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&dsvHeap_)),
        "Failed to create shadow map DSV heap"
    );

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc = {};
    dsvViewDesc.Format        = DXGI_FORMAT_D32_FLOAT;
    dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvViewDesc.Flags         = D3D12_DSV_FLAG_NONE;
    dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(depthBuffer_.Get(), &dsvViewDesc, dsvHandle_);

    // SRV in the global SRV heap (allocated via GraphicsDevice)
    uint32 srvIdx = graphics->AllocateSRVIndex();
    auto descriptorSize = graphics->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto* srvHeap = graphics->GetSRVHeap();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvViewDesc = {};
    srvViewDesc.Format                  = DXGI_FORMAT_R32_FLOAT;
    srvViewDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvViewDesc.Texture2D.MipLevels     = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    srvCpuHandle.ptr += static_cast<SIZE_T>(srvIdx) * descriptorSize;
    device->CreateShaderResourceView(depthBuffer_.Get(), &srvViewDesc, srvCpuHandle);

    srvHandle_ = srvHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandle_.ptr += static_cast<SIZE_T>(srvIdx) * descriptorSize;

    created_ = true;
    Logger::Info("[ShadowMap] Created {}x{} shadow map", resolution, resolution);
}

void ShadowMap::BeginShadowPass(ID3D12GraphicsCommandList* cmdList) {
    if (!created_) return;

    // Already starts in DEPTH_WRITE state (initial state)
    cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle_);
    cmdList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT vp = { 0, 0,
        static_cast<float>(resolution_), static_cast<float>(resolution_),
        0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, static_cast<LONG>(resolution_), static_cast<LONG>(resolution_) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);
}

void ShadowMap::EndShadowPass(ID3D12GraphicsCommandList* cmdList) {
    if (!created_) return;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = depthBuffer_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
}

void ShadowMap::RestoreForNextFrame(ID3D12GraphicsCommandList* cmdList) {
    if (!created_) return;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = depthBuffer_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
}

Matrix4x4 ShadowMap::ComputeLightViewProj(const Vector3& lightDir,
                                           const Vector3& sceneCenter,
                                           float sceneRadius) {
    Vector3 dir = lightDir.Normalize();
    Vector3 lightPos = sceneCenter - dir * (sceneRadius * 2.0f);

    Vector3 up = (std::abs(dir.GetY()) > 0.99f)
        ? Vector3(1, 0, 0) : Vector3(0, 1, 0);

    Matrix4x4 view = Matrix4x4::LookAtLH(lightPos, sceneCenter, up);

    // Symmetrical ortho covering the scene
    float halfSize = sceneRadius * 1.5f;
    Matrix4x4 proj = Matrix4x4::OrthographicLH(
        halfSize * 2.0f, halfSize * 2.0f,
        0.1f, sceneRadius * 6.0f
    );

    return view * proj;
}

Matrix4x4 ShadowMap::ComputeSpotLightViewProj(const Vector3& position,
                                               const Vector3& direction,
                                               float spotAngleRad,
                                               float range) {
    Vector3 dir = direction.Normalize();
    Vector3 target = position + dir;

    Vector3 up = (std::abs(dir.GetY()) > 0.99f)
        ? Vector3(1, 0, 0) : Vector3(0, 1, 0);

    Matrix4x4 view = Matrix4x4::LookAtLH(position, target, up);

    // Use the full cone angle (spotAngle is half-angle, so fov = spotAngle * 2)
    float fov = spotAngleRad * 2.0f;
    // Clamp to reasonable range
    if (fov < 0.1f) fov = 0.1f;
    if (fov > 3.0f) fov = 3.0f;

    Matrix4x4 proj = Matrix4x4::PerspectiveFovLH(fov, 1.0f, 0.1f, range);

    return view * proj;
}

} // namespace UnoEngine
