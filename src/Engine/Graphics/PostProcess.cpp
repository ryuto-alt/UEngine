#include "PostProcess.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cassert>

PostProcess::~PostProcess() {
    Finalize();
}

void PostProcess::Finalize() {
    if (paramsResource_ && paramsData_) {
        paramsResource_->Unmap(0, nullptr);
        paramsData_ = nullptr;
    }

    paramsResource_.Reset();
    renderTargetResource_.Reset();
    rtvDescriptorHeap_.Reset();
    rootSignature_.Reset();
    pipelineState_.Reset();

    srvAllocated_ = false;
    srvIndex_ = 0;
}

void PostProcess::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, EffectType type) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    effectType_ = type;

    CreateRenderTarget();
    CreatePipeline();

    // All param structs are 32 bytes
    paramsResource_ = dxCommon_->CreateBufferResource(32);
    paramsResource_->Map(0, nullptr, &paramsData_);

    switch (effectType_) {
    case EffectType::Horror:
        currentHorrorParams_.time = 0.0f;
        currentHorrorParams_.noiseIntensity = 0.3f;
        currentHorrorParams_.distortionAmount = 0.5f;
        currentHorrorParams_.bloodAmount = 0.4f;
        currentHorrorParams_.vignetteIntensity = 0.0f;
        currentHorrorParams_.fisheyeStrength = 0.0f;
        currentHorrorParams_.fisheyeRadius = 1.5f;
        memcpy(paramsData_, &currentHorrorParams_, sizeof(HorrorParams));
        break;
    case EffectType::TitleNoise:
        currentNoiseParams_ = {};
        currentNoiseParams_.scanlineCount = 400.0f;
        currentNoiseParams_.grainIntensity = 0.06f;
        memcpy(paramsData_, &currentNoiseParams_, sizeof(TitleNoiseParams));
        break;
    case EffectType::PSXRetro:
        currentPSXParams_.screenWidth = static_cast<float>(dxCommon_->GetCurrentWindowWidth());
        currentPSXParams_.screenHeight = static_cast<float>(dxCommon_->GetCurrentWindowHeight());
        currentPSXParams_.targetWidth = 480.0f;
        currentPSXParams_.targetHeight = 360.0f;
        currentPSXParams_.colorDepth = 5;
        currentPSXParams_.enableDithering = 1;
        memcpy(paramsData_, &currentPSXParams_, sizeof(PSXParams));
        break;
    case EffectType::VHS:
        currentVHSParams_.time = 0.0f;
        currentVHSParams_.scanlineIntensity = 0.5f;
        currentVHSParams_.noiseIntensity = 0.3f;
        currentVHSParams_.trackingError = 1.0f;
        currentVHSParams_.chromaticAberration = 1.5f;
        currentVHSParams_.colorBleed = 0.8f;
        currentVHSParams_.sharpness = 0.6f;
        currentVHSParams_.tapeCrease = 0.5f;
        memcpy(paramsData_, &currentVHSParams_, sizeof(VHSParams));
        break;
    case EffectType::CRT:
        currentCRTParams_.cornerRadius = 0.06f;
        currentCRTParams_.curvature = 0.08f;
        currentCRTParams_.vignetteStrength = 0.3f;
        currentCRTParams_.edgeSoftness = 0.008f;
        currentCRTParams_.screenAspect = 16.0f / 9.0f;
        currentCRTParams_.targetAspect = 4.0f / 3.0f;
        memcpy(paramsData_, &currentCRTParams_, sizeof(CRTParams));
        break;
    }
}

void PostProcess::CreateRenderTarget() {
    HRESULT hr;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    hr = dxCommon_->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
    assert(SUCCEEDED(hr));

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = dxCommon_->GetCurrentWindowWidth();
    resourceDesc.Height = dxCommon_->GetCurrentWindowHeight();
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&renderTargetResource_)
    );
    assert(SUCCEEDED(hr));

    rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    dxCommon_->GetDevice()->CreateRenderTargetView(renderTargetResource_.Get(), nullptr, rtvHandle_);

    if (!srvAllocated_) {
        srvIndex_ = srvManager_->Allocate();
        srvAllocated_ = true;
    }
    srvManager_->CreateSRVForTexture2D(srvIndex_, renderTargetResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
    srvGPUHandle_ = srvManager_->GetGPUDescriptorHandle(srvIndex_);
}

void PostProcess::CreatePipeline() {
    HRESULT hr;

    // Root Signature
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    // PSX uses POINT filtering, others use LINEAR
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = (effectType_ == EffectType::PSXRetro)
        ? D3D12_FILTER_MIN_MAG_MIP_POINT
        : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootSignatureDesc.pStaticSamplers = staticSamplers;
    rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    // Compile shaders based on effect type
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");

    const wchar_t* psShaderPath = L"Resources/shaders/Horror.PS.hlsl";
    switch (effectType_) {
    case EffectType::Horror:     psShaderPath = L"Resources/shaders/Horror.PS.hlsl"; break;
    case EffectType::TitleNoise: psShaderPath = L"Resources/shaders/TitleNoise.PS.hlsl"; break;
    case EffectType::PSXRetro:   psShaderPath = L"Resources/shaders/PSXEffect.PS.hlsl"; break;
    case EffectType::VHS:        psShaderPath = L"Resources/shaders/VHS.PS.hlsl"; break;
    case EffectType::CRT:        psShaderPath = L"Resources/shaders/CRT.PS.hlsl"; break;
    }
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(psShaderPath, L"ps_6_0");

    // Pipeline State
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
    pipelineStateDesc.pRootSignature = rootSignature_.Get();
    pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

    pipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    pipelineStateDesc.NumRenderTargets = 1;
    pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateDesc.SampleDesc.Count = 1;
    pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    pipelineStateDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineStateDesc.InputLayout.NumElements = 0;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void PostProcess::PreDraw() {
    OutputDebugStringA("PostProcess::PreDraw - 1: Start\n");
    if (!dxCommon_ || !renderTargetResource_) {
        OutputDebugStringA("ERROR: PostProcess::PreDraw - null pointer detected!\n");
        return;
    }

    OutputDebugStringA("PostProcess::PreDraw - 2: Getting command list\n");
    auto commandList = dxCommon_->GetCommandList();
    if (!commandList) {
        OutputDebugStringA("ERROR: commandList is null!\n");
        return;
    }

    OutputDebugStringA("PostProcess::PreDraw - 3: Clearing RT\n");
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);

    OutputDebugStringA("PostProcess::PreDraw - 4: Getting DSV handle\n");
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);

    OutputDebugStringA("PostProcess::PreDraw - 5: Setting render targets\n");
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle);

    OutputDebugStringA("PostProcess::PreDraw - 6: Getting RT desc\n");
    // Use render target's actual dimensions (not window size) for viewport
    D3D12_RESOURCE_DESC rtDesc = renderTargetResource_->GetDesc();
    UINT width = static_cast<UINT>(rtDesc.Width);
    UINT height = rtDesc.Height;

    OutputDebugStringA("PostProcess::PreDraw - 7: Setting viewport\n");

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<FLOAT>(width);
    viewport.Height = static_cast<FLOAT>(height);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = width;
    scissorRect.bottom = height;

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}

void PostProcess::PostDraw() {
    if (!dxCommon_ || !renderTargetResource_) {
        OutputDebugStringA("ERROR: PostProcess::PostDraw - null pointer detected!\n");
        return;
    }

    auto commandList = dxCommon_->GetCommandList();

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTargetResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    UINT backBufferIndex = dxCommon_->GetBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV = dxCommon_->GetRTVCPUDescriptorHandle(backBufferIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
    commandList->OMSetRenderTargets(1, &backBufferRTV, false, &dsvHandle);

    // Set viewport to back buffer dimensions for final output
    UINT backBufferWidth = dxCommon_->GetCurrentWindowWidth();
    UINT backBufferHeight = dxCommon_->GetCurrentWindowHeight();

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<FLOAT>(backBufferWidth);
    viewport.Height = static_cast<FLOAT>(backBufferHeight);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = backBufferWidth;
    scissorRect.bottom = backBufferHeight;

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());

    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, srvGPUHandle_);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);

    D3D12_RESOURCE_BARRIER barrier2{};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier2.Transition.pResource = renderTargetResource_.Get();
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier2);
}

void PostProcess::PostDrawTo(PostProcess* nextEffect) {
    auto commandList = dxCommon_->GetCommandList();

    // Transition this RT: RENDER_TARGET → SRV
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTargetResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // Clear and set next effect's RT as render target
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    D3D12_CPU_DESCRIPTOR_HANDLE nextRTV = nextEffect->GetRTVHandle();
    commandList->ClearRenderTargetView(nextRTV, clearColor, 0, nullptr);
    commandList->OMSetRenderTargets(1, &nextRTV, false, nullptr);

    // Use next effect's render target actual dimensions for viewport
    D3D12_RESOURCE_DESC nextRtDesc = nextEffect->GetRenderTarget()->GetDesc();
    UINT width = static_cast<UINT>(nextRtDesc.Width);
    UINT height = nextRtDesc.Height;

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<FLOAT>(width);
    viewport.Height = static_cast<FLOAT>(height);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = width;
    scissorRect.bottom = height;

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    // Draw fullscreen triangle with this effect's shader
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetGraphicsRootConstantBufferView(0, paramsResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, srvGPUHandle_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);

    // Transition this RT back: SRV → RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier2{};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier2.Transition.pResource = renderTargetResource_.Get();
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier2);
}

// --- Horror ---

void PostProcess::SetHorrorParams(float time, float noise, float distortion, float blood, float vignette) {
    currentHorrorParams_.time = time;
    currentHorrorParams_.noiseIntensity = noise;
    currentHorrorParams_.distortionAmount = distortion;
    currentHorrorParams_.bloodAmount = blood;
    currentHorrorParams_.vignetteIntensity = vignette;

    if (paramsData_) {
        memcpy(paramsData_, &currentHorrorParams_, sizeof(HorrorParams));
    }
}

void PostProcess::SetFisheyeStrength(float strength) {
    currentHorrorParams_.fisheyeStrength = strength;
    if (paramsData_) {
        memcpy(paramsData_, &currentHorrorParams_, sizeof(HorrorParams));
    }
}

void PostProcess::SetFisheyeRadius(float radius) {
    currentHorrorParams_.fisheyeRadius = radius;
    if (paramsData_) {
        memcpy(paramsData_, &currentHorrorParams_, sizeof(HorrorParams));
    }
}

// --- TitleNoise ---

void PostProcess::SetTitleNoiseParams(float time, float grain, float scanline, float scanlineCount,
                                       float glitch, float glitchFreq, float chromatic, float vignette) {
    currentNoiseParams_.time = time;
    currentNoiseParams_.grainIntensity = grain;
    currentNoiseParams_.scanlineIntensity = scanline;
    currentNoiseParams_.scanlineCount = scanlineCount;
    currentNoiseParams_.glitchIntensity = glitch;
    currentNoiseParams_.glitchFrequency = glitchFreq;
    currentNoiseParams_.chromaticStrength = chromatic;
    currentNoiseParams_.vignetteIntensity = vignette;

    if (paramsData_) {
        memcpy(paramsData_, &currentNoiseParams_, sizeof(TitleNoiseParams));
    }
}

// --- PSX ---

void PostProcess::SetPSXParams(float screenW, float screenH, float targetW, float targetH,
                                int colorDepth, bool dithering) {
    currentPSXParams_.screenWidth = screenW;
    currentPSXParams_.screenHeight = screenH;
    currentPSXParams_.targetWidth = targetW;
    currentPSXParams_.targetHeight = targetH;
    currentPSXParams_.colorDepth = colorDepth;
    currentPSXParams_.enableDithering = dithering ? 1 : 0;

    if (paramsData_) {
        memcpy(paramsData_, &currentPSXParams_, sizeof(PSXParams));
    }
}

void PostProcess::SetVHSParams(float time, float scanline, float noise, float tracking,
                               float chromatic, float bleed, float sharpness, float crease) {
    currentVHSParams_.time = time;
    currentVHSParams_.scanlineIntensity = scanline;
    currentVHSParams_.noiseIntensity = noise;
    currentVHSParams_.trackingError = tracking;
    currentVHSParams_.chromaticAberration = chromatic;
    currentVHSParams_.colorBleed = bleed;
    currentVHSParams_.sharpness = sharpness;
    currentVHSParams_.tapeCrease = crease;

    if (paramsData_) {
        memcpy(paramsData_, &currentVHSParams_, sizeof(VHSParams));
    }
}

void PostProcess::SetCRTParams(float cornerRadius, float curvature, float vignette,
                               float edgeSoftness, float screenAspect, float targetAspect) {
    currentCRTParams_.cornerRadius = cornerRadius;
    currentCRTParams_.curvature = curvature;
    currentCRTParams_.vignetteStrength = vignette;
    currentCRTParams_.edgeSoftness = edgeSoftness;
    currentCRTParams_.screenAspect = screenAspect;
    currentCRTParams_.targetAspect = targetAspect;

    if (paramsData_) {
        memcpy(paramsData_, &currentCRTParams_, sizeof(CRTParams));
    }
}

void PostProcess::ResizeRenderTarget() {
    renderTargetResource_.Reset();
    rtvDescriptorHeap_.Reset();
    CreateRenderTarget();

    // Update PSX screen dimensions if applicable
    if (effectType_ == EffectType::PSXRetro && paramsData_) {
        currentPSXParams_.screenWidth = static_cast<float>(dxCommon_->GetCurrentWindowWidth());
        currentPSXParams_.screenHeight = static_cast<float>(dxCommon_->GetCurrentWindowHeight());
        memcpy(paramsData_, &currentPSXParams_, sizeof(PSXParams));
    }
}
