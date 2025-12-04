#include "pch.h"
#include "ParticleSystem.h"
#include "../Core/Camera.h"
#include "../Core/Logger.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/d3dx12.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace UnoEngine {

ParticleSystem::ParticleSystem() = default;

ParticleSystem::~ParticleSystem() {
    Shutdown();
}

void ParticleSystem::Initialize(GraphicsDevice* graphics, const ParticleSystemConfig& config) {
    graphics_ = graphics;
    config_ = config;

    Logger::Info("[ParticleSystem] Initializing with max particles: %u", config_.maxParticles);

    CreateGPUResources();
    CreateRootSignatures();
    CreateComputePipelines();
    CreateRenderPipeline();

    // 初期化: Dead Listを全インデックスで埋める
    ResetCounters();

    Logger::Info("[ParticleSystem] Initialization complete");
}

void ParticleSystem::Shutdown() {
    emitters_.clear();
    // ComPtrは自動解放
}

void ParticleSystem::CreateGPUResources() {
    auto device = graphics_->GetDevice();
    uint32 maxParticles = config_.maxParticles;

    // パーティクルプール
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(GPUParticle) * maxParticles,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&particlePool_)));
        particlePool_->SetName(L"ParticlePool");
    }

    // Dead List
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(uint32) * maxParticles,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&deadList_)));
        deadList_->SetName(L"ParticleDeadList");
    }

    // Alive List A
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(uint32) * maxParticles,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&aliveListA_)));
        aliveListA_->SetName(L"ParticleAliveListA");
    }

    // Alive List B
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(uint32) * maxParticles,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&aliveListB_)));
        aliveListB_->SetName(L"ParticleAliveListB");
    }

    // Counter Buffer
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(ParticleCounters),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&counterBuffer_)));
        counterBuffer_->SetName(L"ParticleCounters");
    }

    // Counter Readback Buffer
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ParticleCounters));

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&counterReadbackBuffer_)));
    }

    // Indirect Args Buffer
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(
            sizeof(DrawIndirectArgs),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&indirectArgsBuffer_)));
        indirectArgsBuffer_->SetName(L"ParticleIndirectArgs");
    }

    // 定数バッファ
    auto createConstantBuffer = [&](ComPtr<ID3D12Resource>& buffer, size_t size, const wchar_t* name) {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer((size + 255) & ~255);

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&buffer)));
        buffer->SetName(name);
    };

    createConstantBuffer(systemCB_, sizeof(ParticleSystemCB), L"ParticleSystemCB");
    createConstantBuffer(emitterCB_, sizeof(GPUEmitterParams), L"ParticleEmitterCB");
    createConstantBuffer(updateCB_, sizeof(ParticleUpdateCB), L"ParticleUpdateCB");
    createConstantBuffer(renderCB_, sizeof(ParticleRenderCB), L"ParticleRenderCB");
}

void ParticleSystem::CreateRootSignatures() {
    auto device = graphics_->GetDevice();

    // Compute Root Signature
    {
        CD3DX12_DESCRIPTOR_RANGE1 srvRange;
        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0: Depth Buffer

        CD3DX12_ROOT_PARAMETER1 params[9];
        // b0: System CB
        params[0].InitAsConstantBufferView(0);
        // b1: Emitter/Update CB
        params[1].InitAsConstantBufferView(1);
        // u0: Particle Pool
        params[2].InitAsUnorderedAccessView(0);
        // u1: Dead List
        params[3].InitAsUnorderedAccessView(1);
        // u2: Alive List In
        params[4].InitAsUnorderedAccessView(2);
        // u3: Alive List Out
        params[5].InitAsUnorderedAccessView(3);
        // u4: Counters
        params[6].InitAsUnorderedAccessView(4);
        // u5: Indirect Args
        params[7].InitAsUnorderedAccessView(5);
        // t0: Depth Buffer (for collision)
        params[8].InitAsDescriptorTable(1, &srvRange);

        // Static sampler for depth buffer
        CD3DX12_STATIC_SAMPLER_DESC depthSampler(0, D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
        desc.Init_1_1(_countof(params), params, 1, &depthSampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1,
            &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                Logger::Error("[ParticleSystem] Root signature error: %s",
                    static_cast<const char*>(error->GetBufferPointer()));
            }
            ThrowIfFailed(hr);
        }

        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_)));
    }

    // Render Root Signature
    {
        CD3DX12_DESCRIPTOR_RANGE1 srvRanges[2];
        srvRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0: Particle Pool
        srvRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);  // t1: Alive List

        CD3DX12_DESCRIPTOR_RANGE1 texRange;
        texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);  // t2: Texture

        CD3DX12_ROOT_PARAMETER1 params[5];
        params[0].InitAsConstantBufferView(0);  // b0: System CB
        params[1].InitAsConstantBufferView(2);  // b2: Render CB
        params[2].InitAsDescriptorTable(1, &srvRanges[0]);  // t0
        params[3].InitAsDescriptorTable(1, &srvRanges[1]);  // t1
        params[4].InitAsDescriptorTable(1, &texRange);      // t2

        CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
        desc.Init_1_1(_countof(params), params, 1, &sampler,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1,
            &signature, &error);

        ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&renderRootSignature_)));
    }

    // Command Signature for ExecuteIndirect
    {
        D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
        argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

        D3D12_COMMAND_SIGNATURE_DESC desc = {};
        desc.ByteStride = sizeof(DrawIndirectArgs);
        desc.NumArgumentDescs = 1;
        desc.pArgumentDescs = &argDesc;

        ThrowIfFailed(device->CreateCommandSignature(&desc, nullptr,
            IID_PPV_ARGS(&commandSignature_)));
    }
}

void ParticleSystem::CreateComputePipelines() {
    auto device = graphics_->GetDevice();

    // シェーダーコンパイル
    emitCS_.CompileFromFile(L"Shaders/Particle/ParticleEmitCS.hlsl", ShaderStage::Compute, "main");
    updateCS_.CompileFromFile(L"Shaders/Particle/ParticleUpdateCS.hlsl", ShaderStage::Compute, "main");
    buildArgsCS_.CompileFromFile(L"Shaders/Particle/ParticleUpdateCS.hlsl", ShaderStage::Compute, "BuildIndirectArgs");

    // Emit PSO
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = computeRootSignature_.Get();
        desc.CS = emitCS_.GetBytecodeDesc();

        ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&emitPSO_)));
    }

    // Update PSO
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = computeRootSignature_.Get();
        desc.CS = updateCS_.GetBytecodeDesc();

        ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&updatePSO_)));
    }

    // Build Args PSO
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = computeRootSignature_.Get();
        desc.CS = buildArgsCS_.GetBytecodeDesc();

        ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&buildArgsPSO_)));
    }
}

void ParticleSystem::CreateRenderPipeline() {
    auto device = graphics_->GetDevice();

    // シェーダーコンパイル
    billboardVS_.CompileFromFile(L"Shaders/Particle/ParticleBillboardVS.hlsl", ShaderStage::Vertex, "main");
    particlePS_.CompileFromFile(L"Shaders/Particle/ParticlePS.hlsl", ShaderStage::Pixel, "main");

    // 共通のPSO設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc = {};
    baseDesc.pRootSignature = renderRootSignature_.Get();
    baseDesc.VS = billboardVS_.GetBytecodeDesc();
    baseDesc.PS = particlePS_.GetBytecodeDesc();
    baseDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    baseDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // 両面描画
    baseDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    baseDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;  // 深度書き込み無効
    baseDesc.SampleMask = UINT_MAX;
    baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    baseDesc.NumRenderTargets = 1;
    baseDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    baseDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    baseDesc.SampleDesc.Count = 1;

    // Additive Blend
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;
        desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&renderPSO_Additive_)));
    }

    // Alpha Blend
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;
        desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&renderPSO_AlphaBlend_)));
    }

    // Multiply
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;
        desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
        desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&renderPSO_Multiply_)));
    }
}

ParticleEmitter* ParticleSystem::CreateEmitter(const std::string& name) {
    EmitterConfig config;
    config.name = name;
    return CreateEmitter(config);
}

ParticleEmitter* ParticleSystem::CreateEmitter(const EmitterConfig& config) {
    auto emitter = std::make_unique<ParticleEmitter>(config);
    ParticleEmitter* ptr = emitter.get();
    emitters_.push_back(std::move(emitter));
    return ptr;
}

void ParticleSystem::RemoveEmitter(ParticleEmitter* emitter) {
    auto it = std::find_if(emitters_.begin(), emitters_.end(),
        [emitter](const auto& e) { return e.get() == emitter; });
    if (it != emitters_.end()) {
        emitters_.erase(it);
    }
}

void ParticleSystem::RemoveAllEmitters() {
    emitters_.clear();
}

ParticleEmitter* ParticleSystem::GetEmitter(const std::string& name) {
    for (auto& emitter : emitters_) {
        if (emitter->GetConfig().name == name) {
            return emitter.get();
        }
    }
    return nullptr;
}

ParticleEmitter* ParticleSystem::GetEmitter(size_t index) {
    if (index < emitters_.size()) {
        return emitters_[index].get();
    }
    return nullptr;
}

void ParticleSystem::Update(float deltaTime) {
    totalTime_ += deltaTime;
    frameIndex_++;

    // エミッター更新
    for (auto& emitter : emitters_) {
        emitter->Update(deltaTime);
    }
}

void ParticleSystem::Render(Camera* camera, ID3D12Resource* depthBuffer) {
    if (!camera) return;

    auto commandList = graphics_->GetCommandList();
    float deltaTime = 1.0f / 60.0f;  // TODO: 実際のdeltaTimeを渡す

    // システム定数バッファ更新
    UpdateSystemConstantBuffer(camera, deltaTime);

    // パーティクル発生
    for (auto& emitter : emitters_) {
        uint32 emitCount = emitter->CalculateEmitCount(deltaTime);
        if (emitCount > 0) {
            // GPUエミッターパラメータ設定
            GPUEmitterParams params = {};
            params.position = emitter->GetPosition();
            params.emitRate = emitter->GetConfig().emitRate;
            params.minVelocity = { 0, 0, 0 };  // TODO: カーブから計算
            params.maxVelocity = { 0, 2, 0 };
            params.deltaTime = deltaTime;
            params.time = emitter->GetTime();
            params.minLifetime = emitter->GetConfig().startLifetime.constantMin;
            params.maxLifetime = emitter->GetConfig().startLifetime.constantMax;
            params.minSize = emitter->GetConfig().startSize.constantMin;
            params.maxSize = emitter->GetConfig().startSize.constantMax;
            params.startColor = emitter->GetConfig().startColor.colorMin;
            params.gravity = gravity_;
            params.drag = drag_;
            params.emitterID = emitter->GetEmitterID();
            params.maxParticles = config_.maxParticles;
            params.emitShape = static_cast<uint32>(emitter->GetConfig().shape.shape);
            params.shapeRadius = emitter->GetConfig().shape.radius;
            params.coneAngle = emitter->GetConfig().shape.coneAngle;

            EmitParticles(emitCount, params);
        }
    }

    // パーティクル更新
    UpdateParticles();

    // Indirect引数構築
    BuildIndirectArgs();

    // レンダリング
    // TODO: ExecuteIndirectでの描画を実装
}

void ParticleSystem::Play() {
    for (auto& emitter : emitters_) {
        emitter->Play();
    }
}

void ParticleSystem::Pause() {
    for (auto& emitter : emitters_) {
        emitter->Pause();
    }
}

void ParticleSystem::Stop() {
    for (auto& emitter : emitters_) {
        emitter->Stop();
    }
}

void ParticleSystem::Restart() {
    for (auto& emitter : emitters_) {
        emitter->Restart();
    }
    ResetCounters();
}

void ParticleSystem::ResetCounters() {
    // GPU上でカウンターを初期化するためのCompute Shaderを実行
    // またはCPUから初期値をアップロード
    auto commandList = graphics_->GetCommandList();

    // Dead Listを全インデックスで初期化
    // TODO: 初期化用Compute Shaderまたはコピー操作
}

void ParticleSystem::EmitParticles(uint32 emitCount, const GPUEmitterParams& emitterParams) {
    auto commandList = graphics_->GetCommandList();

    // エミッターCB更新
    void* mapped = nullptr;
    emitterCB_->Map(0, nullptr, &mapped);
    memcpy(mapped, &emitterParams, sizeof(GPUEmitterParams));
    emitterCB_->Unmap(0, nullptr);

    // Compute Shader実行
    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(emitPSO_.Get());

    commandList->SetComputeRootConstantBufferView(0, systemCB_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(1, emitterCB_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(2, particlePool_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(3, deadList_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(4,
        useAliveListA_ ? aliveListA_->GetGPUVirtualAddress() : aliveListB_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(5,
        useAliveListA_ ? aliveListB_->GetGPUVirtualAddress() : aliveListA_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(6, counterBuffer_->GetGPUVirtualAddress());

    uint32 threadGroups = (emitCount + 63) / 64;
    commandList->Dispatch(threadGroups, 1, 1);
}

void ParticleSystem::UpdateParticles() {
    auto commandList = graphics_->GetCommandList();

    // 更新CB設定
    ParticleUpdateCB updateParams = {};
    updateParams.gravity = gravity_;
    updateParams.drag = drag_;
    updateParams.aliveCountIn = aliveParticleCount_;
    updateParams.collisionEnabled = config_.enableCollision ? 1 : 0;
    updateParams.collisionBounce = 0.5f;
    updateParams.collisionLifetimeLoss = 0.1f;

    void* mapped = nullptr;
    updateCB_->Map(0, nullptr, &mapped);
    memcpy(mapped, &updateParams, sizeof(ParticleUpdateCB));
    updateCB_->Unmap(0, nullptr);

    // UAVバリア
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(particlePool_.Get());
    commandList->ResourceBarrier(1, &barrier);

    // Update Shader実行
    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(updatePSO_.Get());

    commandList->SetComputeRootConstantBufferView(0, systemCB_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(1, updateCB_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(2, particlePool_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(3, deadList_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(4,
        useAliveListA_ ? aliveListA_->GetGPUVirtualAddress() : aliveListB_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(5,
        useAliveListA_ ? aliveListB_->GetGPUVirtualAddress() : aliveListA_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(6, counterBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(7, indirectArgsBuffer_->GetGPUVirtualAddress());

    uint32 threadGroups = (aliveParticleCount_ + 255) / 256;
    if (threadGroups > 0) {
        commandList->Dispatch(threadGroups, 1, 1);
    }

    // ダブルバッファ切り替え
    useAliveListA_ = !useAliveListA_;
}

void ParticleSystem::BuildIndirectArgs() {
    auto commandList = graphics_->GetCommandList();

    // UAVバリア
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(counterBuffer_.Get());
    commandList->ResourceBarrier(1, &barrier);

    // Build Args Shader実行
    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(buildArgsPSO_.Get());

    commandList->SetComputeRootUnorderedAccessView(6, counterBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(7, indirectArgsBuffer_->GetGPUVirtualAddress());

    commandList->Dispatch(1, 1, 1);
}

void ParticleSystem::UpdateSystemConstantBuffer(Camera* camera, float deltaTime) {
    ParticleSystemCB cb = {};

    // カメラ行列
    auto view = camera->GetViewMatrix();
    auto proj = camera->GetProjectionMatrix();

    // Matrix4x4からFloat4x4への変換（GetElement使用）
    auto matrixToFloat4x4 = [](const Matrix4x4& mat) -> Float4x4 {
        return Float4x4(
            mat.GetElement(0, 0), mat.GetElement(0, 1), mat.GetElement(0, 2), mat.GetElement(0, 3),
            mat.GetElement(1, 0), mat.GetElement(1, 1), mat.GetElement(1, 2), mat.GetElement(1, 3),
            mat.GetElement(2, 0), mat.GetElement(2, 1), mat.GetElement(2, 2), mat.GetElement(2, 3),
            mat.GetElement(3, 0), mat.GetElement(3, 1), mat.GetElement(3, 2), mat.GetElement(3, 3)
        );
    };

    cb.viewMatrix = matrixToFloat4x4(view);
    cb.projMatrix = matrixToFloat4x4(proj);

    // ViewProj計算
    auto viewProj = view * proj;
    cb.viewProjMatrix = matrixToFloat4x4(viewProj);

    // カメラ情報
    const auto& camPos = camera->GetPosition();
    cb.cameraPosition = { camPos.GetX(), camPos.GetY(), camPos.GetZ() };

    // カメラのRight/Upベクトル（ビルボード用）
    cb.cameraRight = { view.GetElement(0, 0), view.GetElement(1, 0), view.GetElement(2, 0) };
    cb.cameraUp = { view.GetElement(0, 1), view.GetElement(1, 1), view.GetElement(2, 1) };

    cb.totalTime = totalTime_;
    cb.deltaTime = deltaTime;
    cb.frameIndex = frameIndex_;

    // アップロード
    void* mapped = nullptr;
    systemCB_->Map(0, nullptr, &mapped);
    memcpy(mapped, &cb, sizeof(ParticleSystemCB));
    systemCB_->Unmap(0, nullptr);
}

} // namespace UnoEngine
