#include "pch.h"
#include "GrassRenderer.h"
#include "../Core/Logger.h"
#include <filesystem>
#include <random>
#include <vector>
#include <cmath>

namespace UnoEngine {

namespace {

void StoreTransposedMatrix(Float4x4& dest, const Matrix4x4& src) {
    Matrix4x4 transposed = src.Transpose();
    transposed.ToFloatArray(reinterpret_cast<float*>(&dest));
}

} // anonymous namespace

void GrassRenderer::Initialize(GraphicsDevice* graphics) {
    graphics_ = graphics;
    auto* device = graphics_->GetDevice();

    pipeline_.Initialize(device, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    transformBuffer_.Create(device, 4);

    Logger::Info("[GrassRenderer] 初期化完了");
}

void GrassRenderer::CreateNoiseTextures(GraphicsDevice* graphics, ID3D12GraphicsCommandList* cmdList) {
    if (noiseTexturesCreated_) return;

    constexpr int SIZE = 256;
    std::vector<uint32> pixels(SIZE * SIZE);

    // スムーズドノイズ生成
    auto generateNoise = [&](uint32 seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // 粗いランダム値
        std::vector<float> raw(SIZE * SIZE);
        for (auto& v : raw) v = dist(rng);

        // スムージングパス（5x5ボックスフィルタ、ラッピング対応）
        std::vector<float> smoothed(SIZE * SIZE);
        for (int y = 0; y < SIZE; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                float sum = 0.0f;
                int count = 0;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        int nx = (x + dx + SIZE) % SIZE;  // ラップ
                        int ny = (y + dy + SIZE) % SIZE;
                        sum += raw[ny * SIZE + nx];
                        count++;
                    }
                }
                smoothed[y * SIZE + x] = sum / static_cast<float>(count);
            }
        }

        // 2回目のスムージング
        std::vector<float> smoothed2(SIZE * SIZE);
        for (int y = 0; y < SIZE; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                float sum = 0.0f;
                int count = 0;
                for (int dy = -3; dy <= 3; ++dy) {
                    for (int dx = -3; dx <= 3; ++dx) {
                        int nx = (x + dx + SIZE) % SIZE;
                        int ny = (y + dy + SIZE) % SIZE;
                        sum += smoothed[ny * SIZE + nx];
                        count++;
                    }
                }
                smoothed2[y * SIZE + x] = sum / static_cast<float>(count);
            }
        }

        // RGBA8ピクセルに変換
        for (int i = 0; i < SIZE * SIZE; ++i) {
            uint8_t v = static_cast<uint8_t>(smoothed2[i] * 255.0f);
            pixels[i] = (255u << 24) | (static_cast<uint32>(v) << 16) | (static_cast<uint32>(v) << 8) | v;
        }
    };

    // 連続したSRVインデックスを確保
    noiseTexture1SRVIndex_ = graphics->AllocateSRVIndex();
    noiseTexture2SRVIndex_ = graphics->AllocateSRVIndex();
    windNoiseSRVIndex_     = graphics->AllocateSRVIndex();

    // ノイズテクスチャ1（カラーバリエーション用）
    generateNoise(12345);
    noiseTexture1_.CreateFromData(graphics, cmdList, pixels.data(), SIZE, SIZE, noiseTexture1SRVIndex_, false);

    // ノイズテクスチャ2（カラーバリエーション用、異なるシード）
    generateNoise(67890);
    noiseTexture2_.CreateFromData(graphics, cmdList, pixels.data(), SIZE, SIZE, noiseTexture2SRVIndex_, false);

    // 風ノイズテクスチャ
    generateNoise(54321);
    windNoiseTexture_.CreateFromData(graphics, cmdList, pixels.data(), SIZE, SIZE, windNoiseSRVIndex_, false);

    noiseTexturesCreated_ = true;
    Logger::Info("[GrassRenderer] ノイズテクスチャ3枚生成 (SRV: {}, {}, {})",
                 noiseTexture1SRVIndex_, noiseTexture2SRVIndex_, windNoiseSRVIndex_);
}

void GrassRenderer::CreateFallbackTexture(GraphicsDevice* graphics, ID3D12GraphicsCommandList* cmdList) {
    uint32 whitePixel = 0xFFFFFFFF;
    if (!srvAllocated_) {
        grassTextureSRVIndex_ = graphics->AllocateSRVIndex();
        srvAllocated_ = true;
    }
    grassTexture_.CreateFromData(graphics, cmdList, &whitePixel, 1, 1, grassTextureSRVIndex_, false);
    hasTexture_ = true;
    texturePath_.clear();

    // ノイズテクスチャも一緒に生成
    CreateNoiseTextures(graphics, cmdList);

    Logger::Info("[GrassRenderer] フォールバック白テクスチャ作成 (SRV: {})", grassTextureSRVIndex_);
}

bool GrassRenderer::LoadTextureFromFile(const std::string& filepath) {
    namespace fs = std::filesystem;

    if (!fs::exists(filepath)) {
        Logger::Warning("[GrassRenderer] テクスチャファイルが見つかりません: {}", filepath);
        return false;
    }

    if (!srvAllocated_) {
        grassTextureSRVIndex_ = graphics_->AllocateSRVIndex();
        srvAllocated_ = true;
    }

    // 既存テクスチャリソースを解放
    grassTexture_ = Texture2D();

    auto* cmdList = graphics_->GetCommandList();
    std::wstring wpath(filepath.begin(), filepath.end());
    grassTexture_.LoadFromFile(graphics_, cmdList, wpath, grassTextureSRVIndex_);
    hasTexture_ = true;
    texturePath_ = filepath;

    // ノイズテクスチャも生成（まだなら）
    CreateNoiseTextures(graphics_, cmdList);

    Logger::Info("[GrassRenderer] 草テクスチャ読み込み完了: {} (SRV: {})", filepath, grassTextureSRVIndex_);
    return true;
}

void GrassRenderer::Render(const RenderView& view,
                           const Matrix4x4& lightViewProj,
                           const Matrix4x4 spotLightViewProjs[4],
                           int spotShadowCount,
                           D3D12_GPU_VIRTUAL_ADDRESS lightCBAddr,
                           const ShadowMap& shadowMap,
                           const ShadowMap spotShadowMaps[4],
                           GrassSystem& grassSystem) {
    if (grassSystem.GetInstanceCount() == 0 || !hasTexture_ || !noiseTexturesCreated_) return;

    auto* cmdList = graphics_->GetCommandList();
    auto* heap = graphics_->GetSRVHeap();

    // インスタンスバッファ更新
    grassSystem.UpdateInstanceBuffer(cmdList);

    // ルートシグネチャ & PSO設定
    cmdList->SetGraphicsRootSignature(pipeline_.GetRootSignature());
    ID3D12DescriptorHeap* heaps[] = { heap };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetPipelineState(pipeline_.GetPipelineState());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 定数バッファ
    auto viewMatrix = view.camera->GetViewMatrix();
    auto projMatrix = view.camera->GetProjectionMatrix();
    auto camPos = view.camera->GetPosition();

    GrassTransformCB transformData = {};
    StoreTransposedMatrix(transformData.view, viewMatrix);
    StoreTransposedMatrix(transformData.projection, projMatrix);
    StoreTransposedMatrix(transformData.lightViewProj, lightViewProj);
    for (int i = 0; i < 4; ++i) {
        StoreTransposedMatrix(transformData.spotLightViewProj[i], spotLightViewProjs[i]);
    }

    // カメラ & 時間
    transformData.cameraPos = Float3(camPos.GetX(), camPos.GetY(), camPos.GetZ());
    transformData.time = totalTime_;

    // カラー
    transformData.bottomColor = bottomColor_;
    transformData.topColor = topColor_;
    transformData.colorVar1 = colorVar1_;
    transformData.colorVar2 = colorVar2_;

    // 風
    transformData.windSpeed = windSpeed_;
    transformData.windDis = windDis_;
    transformData.noiseStrength = noiseStrength_;
    transformData.displaceStrength = displaceStrength_;

    // 風影
    transformData.windShadowColor = windShadowColor_;
    transformData.windShadowStrength = windShadowStrength_;

    // インタラクション
    transformData.interactingObjPos = interactingObjPos_;
    transformData.flattenRadius = flattenRadius_;
    transformData.flattenStrength = flattenStrength_;
    transformData.flattenFloor = flattenFloor_;

    // ノイズスケール
    transformData.noise1Scale = noise1Scale_;
    transformData.noise2Scale = noise2Scale_;
    transformData.windNoiseScale = windNoiseScale_;
    transformData.windNoisePanSpeedX = windNoisePanSpeedX_;
    transformData.windNoisePanSpeedY = windNoisePanSpeedY_;
    transformData.noiseFloor = noiseFloor_;
    transformData.windNoiseScaleStrength = windNoiseScaleStrength_;
    transformData.combinedNoiseMinScale = combinedNoiseMinScale_;
    transformData.combinedNoiseMaxScale = combinedNoiseMaxScale_;
    transformData.invertCombinedNoise = invertCombinedNoise_ ? 1.0f : 0.0f;

    // 風影詳細
    transformData.windShadowDispThreshold = windShadowDispThreshold_;
    transformData.windShadowSmoothing = windShadowSmoothing_;
    transformData.flattenShadowStrength = flattenShadowStrength_;

    // サイズ & 描画モード
    transformData.baseWidth = baseWidth_;
    transformData.baseHeight = baseHeight_;
    transformData.useGodotShading = useGodotShading_ ? 1.0f : 0.0f;

    D3D12_GPU_VIRTUAL_ADDRESS transformAddr = transformBuffer_.Update(transformData);

    // ルートパラメータ設定
    uint32 srvIncSize = graphics_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // [0] Transform CB
    cmdList->SetGraphicsRootConstantBufferView(0, transformAddr);

    // [1] Grass texture
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle = heap->GetGPUDescriptorHandleForHeapStart();
    texHandle.ptr += static_cast<uint64>(grassTextureSRVIndex_) * srvIncSize;
    cmdList->SetGraphicsRootDescriptorTable(1, texHandle);

    // [2] Light CB
    cmdList->SetGraphicsRootConstantBufferView(2, lightCBAddr);

    // [3] Shadow map
    cmdList->SetGraphicsRootDescriptorTable(3, shadowMap.GetSRVHandle());

    // [4] Spot shadow maps
    cmdList->SetGraphicsRootDescriptorTable(4, spotShadowMaps[0].GetSRVHandle());

    // [5] Noise textures (t6, t7, t8 - 連続SRV)
    D3D12_GPU_DESCRIPTOR_HANDLE noiseHandle = heap->GetGPUDescriptorHandleForHeapStart();
    noiseHandle.ptr += static_cast<uint64>(noiseTexture1SRVIndex_) * srvIncSize;
    cmdList->SetGraphicsRootDescriptorTable(5, noiseHandle);

    // 頂点バッファ設定: Slot0 = 草メッシュ, Slot1 = インスタンスデータ
    D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {
        grassSystem.GetVertexBufferView(),
        grassSystem.GetInstanceBufferView()
    };
    cmdList->IASetVertexBuffers(0, 2, vbViews);
    auto ibView = grassSystem.GetIndexBufferView();
    cmdList->IASetIndexBuffer(&ibView);

    // インスタンシング描画！
    cmdList->DrawIndexedInstanced(
        grassSystem.GetIndexCount(),
        grassSystem.GetInstanceCount(),
        0, 0, 0);
}

} // namespace UnoEngine
