#include "pch.h"
#include "VideoPlayerComponent.h"
#include "../Core/GameObject.h"
#include "../Graphics/GraphicsDevice.h"
#include "../Graphics/MeshRenderer.h"
#include "../Graphics/Material.h"
#include "../Resource/StaticModelImporter.h"

namespace UnoEngine {

void VideoPlayerComponent::Awake() {
    m_decoder = std::make_unique<VideoDecoder>();
    m_videoTexture = std::make_unique<VideoTexture>();
}

void VideoPlayerComponent::Start() {
    if (!m_videoPath.empty() && m_graphics) {
        LoadVideo(m_videoPath);
        if (!m_targetMaterialName.empty()) {
            SetTargetMaterialByName(m_targetMaterialName);
        }
    }
}

void VideoPlayerComponent::OnUpdate(float deltaTime) {
    if (!m_isPlaying || m_isPaused || !m_decoder || !m_decoder->IsOpen()) {
        return;
    }

    m_frameTimer += deltaTime;

    if (m_frameTimer >= m_frameInterval) {
        m_frameTimer -= m_frameInterval;

        if (m_decoder->DecodeNextFrame()) {
            StageVideoFrame();
        } else if (m_decoder->IsEndOfFile()) {
            Stop();
        }
    }
}

void VideoPlayerComponent::OnDestroy() {
    Stop();

    if (m_targetMaterial) {
        m_targetMaterial->ClearDynamicTexture();
    }

    m_decoder.reset();
    m_videoTexture.reset();
}

bool VideoPlayerComponent::LoadVideo(const std::string& filepath) {
    if (!m_decoder) {
        return false;
    }

    Stop();

    if (!m_decoder->Open(filepath)) {
        return false;
    }

    m_videoPath = filepath;
    m_frameInterval = 1.0 / m_decoder->GetFrameRate();
    m_needsTextureInit = true;

    return true;
}

void VideoPlayerComponent::Play() {
    if (!m_decoder || !m_decoder->IsOpen()) {
        return;
    }

    if (m_needsTextureInit && m_graphics) {
        uint32 srvIndex = m_graphics->AllocateSRVIndex();
        m_videoTexture->Create(
            m_graphics,
            m_decoder->GetWidth(),
            m_decoder->GetHeight(),
            srvIndex);
        m_needsTextureInit = false;

        if (m_targetMaterial) {
            m_targetMaterial->SetDynamicTextureSRVIndex(srvIndex);
        }
    }

    m_isPlaying = true;
    m_isPaused = false;
    m_frameTimer = m_frameInterval;
}

void VideoPlayerComponent::Pause() {
    m_isPaused = true;
}

void VideoPlayerComponent::Stop() {
    m_isPlaying = false;
    m_isPaused = false;
    m_frameTimer = 0.0;

    if (m_decoder && m_decoder->IsOpen()) {
        m_decoder->Seek(0.0);
    }
}

void VideoPlayerComponent::SetLooping(bool loop) {
    if (m_decoder) {
        m_decoder->SetLooping(loop);
    }
}

void VideoPlayerComponent::Seek(double timeInSeconds) {
    if (m_decoder && m_decoder->IsOpen()) {
        m_decoder->Seek(timeInSeconds);
    }
}

void VideoPlayerComponent::SetTargetMaterialByName(const std::string& materialName) {
    m_targetMaterialName = materialName;

    if (!gameObject_) {
        return;
    }

    auto meshRenderer = gameObject_->GetComponent<MeshRenderer>();
    if (!meshRenderer || !meshRenderer->HasModel()) {
        return;
    }

    auto& meshes = meshRenderer->GetMeshes();
    for (auto& mesh : meshes) {
        if (mesh.HasMaterial()) {
            auto material = const_cast<Material*>(mesh.GetMaterial());
            if (material && material->GetData().name == materialName) {
                m_targetMaterial = material;

                if (m_videoTexture && m_videoTexture->IsValid()) {
                    m_targetMaterial->SetDynamicTextureSRVIndex(m_videoTexture->GetSRVIndex());
                }
                return;
            }
        }
    }
}

uint32 VideoPlayerComponent::GetVideoTextureSRVIndex() const {
    return m_videoTexture ? m_videoTexture->GetSRVIndex() : 0;
}

void VideoPlayerComponent::StageVideoFrame() {
    if (!m_videoTexture || !m_videoTexture->IsValid() || !m_decoder) {
        return;
    }

    m_videoTexture->StageFrame(
        m_decoder->GetFrameData(),
        m_decoder->GetRowPitch());
}

void VideoPlayerComponent::UploadVideoFrame(ID3D12GraphicsCommandList* commandList) {
    if (!m_videoTexture || !commandList) {
        return;
    }

    m_videoTexture->UploadStagedFrame(commandList);
}

bool VideoPlayerComponent::HasPendingFrame() const {
    return m_videoTexture && m_videoTexture->HasPendingFrame();
}

} // namespace UnoEngine
