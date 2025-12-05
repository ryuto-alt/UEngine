#pragma once

#include "Component.h"
#include "Camera.h"
#include "../Math/Math.h"
#include "../PostProcess/PostProcessType.h"
#include <vector>

namespace UnoEngine {

/// CameraComponent - GameObjectにアタッチ可能なカメラコンポーネント
/// Unityと同様に、シーン内のカメラを管理する
class CameraComponent : public Component {
public:
    CameraComponent() = default;
    ~CameraComponent() override = default;

    // Component lifecycle
    void Awake() override;
    void Start() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    // カメラ設定
    void SetPerspective(float fovY, float aspect, float nearZ, float farZ);
    void SetOrthographic(float width, float height, float nearZ, float farZ);

    // プロパティ
    float GetFieldOfView() const { return fovY_; }
    void SetFieldOfView(float fov) { fovY_ = fov; updateProjection_ = true; }

    float GetAspectRatio() const { return aspect_; }
    void SetAspectRatio(float aspect) { aspect_ = aspect; updateProjection_ = true; }

    float GetNearClip() const { return nearZ_; }
    void SetNearClip(float nearZ) { nearZ_ = nearZ; updateProjection_ = true; }

    float GetFarClip() const { return farZ_; }
    void SetFarClip(float farZ) { farZ_ = farZ; updateProjection_ = true; }

    bool IsOrthographic() const { return isOrthographic_; }
    void SetOrthographic(bool ortho) { isOrthographic_ = ortho; updateProjection_ = true; }

    // カメラ優先度（値が大きいほど優先）
    int GetPriority() const { return priority_; }
    void SetPriority(int priority) { priority_ = priority; }

    // メインカメラ判定
    bool IsMain() const { return isMain_; }
    void SetMain(bool main) { isMain_ = main; }

    // 内部カメラへのアクセス
    Camera* GetCamera() { return &camera_; }
    const Camera* GetCamera() const { return &camera_; }

    // ビュー/プロジェクション行列
    const Matrix4x4& GetViewMatrix();
    const Matrix4x4& GetProjectionMatrix() const { return camera_.GetProjectionMatrix(); }
    Matrix4x4 GetViewProjectionMatrix();

    // Frustum corners (for visualization)
    void GetFrustumCorners(Vector3 outNearCorners[4], Vector3 outFarCorners[4]) const;

    // Post Processing
    bool IsPostProcessEnabled() const { return postProcessEnabled_; }
    void SetPostProcessEnabled(bool enabled) { postProcessEnabled_ = enabled; }

    // 複数エフェクトチェーン
    const std::vector<PostProcessType>& GetPostProcessEffects() const { return postProcessEffects_; }
    void SetPostProcessEffects(const std::vector<PostProcessType>& effects) { postProcessEffects_ = effects; }
    void AddPostProcessEffect(PostProcessType effect);
    void RemovePostProcessEffect(PostProcessType effect);
    bool HasPostProcessEffect(PostProcessType effect) const;

    // 旧API互換（単一エフェクト）
    PostProcessType GetPostProcessEffect() const { return postProcessEffects_.empty() ? PostProcessType::None : postProcessEffects_[0]; }
    void SetPostProcessEffect(PostProcessType effect);

    float GetPostProcessIntensity() const { return postProcessIntensity_; }
    void SetPostProcessIntensity(float intensity) { postProcessIntensity_ = intensity; }

    // Effect-specific parameters
    const VignetteParams& GetVignetteParams() const { return vignetteParams_; }
    void SetVignetteParams(const VignetteParams& params) { vignetteParams_ = params; }

    const FisheyeParams& GetFisheyeParams() const { return fisheyeParams_; }
    void SetFisheyeParams(const FisheyeParams& params) { fisheyeParams_ = params; }

    const GrayscaleParams& GetGrayscaleParams() const { return grayscaleParams_; }
    void SetGrayscaleParams(const GrayscaleParams& params) { grayscaleParams_ = params; }

private:
    void UpdateCameraTransform();
    void UpdateProjectionMatrix();

    Camera camera_;

    // 投影設定
    float fovY_ = 60.0f * 0.0174533f;  // 60度 (ラジアン)
    float aspect_ = 16.0f / 9.0f;
    float nearZ_ = 0.1f;
    float farZ_ = 1000.0f;
    float orthoWidth_ = 10.0f;
    float orthoHeight_ = 10.0f;
    bool isOrthographic_ = false;
    bool updateProjection_ = true;

    // カメラ設定
    int priority_ = 0;
    bool isMain_ = false;

    // Post Processing設定
    bool postProcessEnabled_ = false;
    std::vector<PostProcessType> postProcessEffects_;
    float postProcessIntensity_ = 1.0f;

    // Effect-specific parameters
    VignetteParams vignetteParams_;
    FisheyeParams fisheyeParams_;
    GrayscaleParams grayscaleParams_;
};

} // namespace UnoEngine
