#pragma once

#include "Component.h"
#include "Camera.h"
#include "../Math/Math.h"
#include "../PostProcess/PostProcessType.h"
#include <vector>
#include <string>

namespace UnoEngine {

// カメラ視点モード
enum class CameraViewMode {
    Free,           // 自由カメラ（追従なし）
    FirstPerson,    // 一人称視点
    ThirdPerson     // 三人称視点
};

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

    const PS1Params& GetPS1Params() const { return ps1Params_; }
    void SetPS1Params(const PS1Params& params) { ps1Params_ = params; }

    const ChromaticAberrationParams& GetChromaticAberrationParams() const { return chromaticAberrationParams_; }
    void SetChromaticAberrationParams(const ChromaticAberrationParams& params) { chromaticAberrationParams_ = params; }

    // カメラ追従設定
    CameraViewMode GetViewMode() const { return viewMode_; }
    void SetViewMode(CameraViewMode mode) { viewMode_ = mode; }

    const std::string& GetFollowTargetName() const { return followTargetName_; }
    void SetFollowTargetName(const std::string& name) { followTargetName_ = name; }

    // 三人称視点設定
    float GetFollowDistance() const { return followDistance_; }
    void SetFollowDistance(float distance) { followDistance_ = distance; }

    float GetFollowHeight() const { return followHeight_; }
    void SetFollowHeight(float height) { followHeight_ = height; }

    float GetFollowPitch() const { return followPitch_; }
    void SetFollowPitch(float pitch) { followPitch_ = pitch; }

    // 一人称視点オフセット
    const Vector3& GetFirstPersonOffset() const { return firstPersonOffset_; }
    void SetFirstPersonOffset(const Vector3& offset) { firstPersonOffset_ = offset; }

    // 一人称視点マウス感度
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    void SetMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

    // 一人称視点移動速度
    float GetFirstPersonMoveSpeed() const { return firstPersonMoveSpeed_; }
    void SetFirstPersonMoveSpeed(float speed) { firstPersonMoveSpeed_ = speed; }

    // 一人称視点でターゲットモデルを非表示にするか
    bool GetHideTargetInFirstPerson() const { return hideTargetInFirstPerson_; }
    void SetHideTargetInFirstPerson(bool hide) { hideTargetInFirstPerson_ = hide; }

    // スムーズ追従
    float GetFollowSmoothness() const { return followSmoothness_; }
    void SetFollowSmoothness(float smoothness) { followSmoothness_ = smoothness; }

    // シーン参照（ターゲット検索用）
    void SetScene(class Scene* scene) { scene_ = scene; }

    // 再生状態（編集中はマウスルック無効）
    void SetPlaying(bool playing) { isPlaying_ = playing; }

    // マウスロック状態（GameViewクリック時にtrueになる）
    void SetMouseLocked(bool locked, int lockX = 0, int lockY = 0) {
        mouseLocked_ = locked;
        mouseLockX_ = lockX;
        mouseLockY_ = lockY;
    }
    bool IsMouseLocked() const { return mouseLocked_; }

    // カメラのYaw/Pitch取得（Lua用）
    float GetCameraYaw() const { return cameraYaw_; }
    float GetCameraPitch() const { return cameraPitch_; }

    // 一人称視点で除外すべきGameObjectを返す
    GameObject* GetFirstPersonExcludeTarget() const;

private:
    void UpdateFollowCamera(float deltaTime);
    GameObject* FindFollowTarget() const;
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
    PS1Params ps1Params_;
    ChromaticAberrationParams chromaticAberrationParams_;

    // カメラ追従設定
    CameraViewMode viewMode_ = CameraViewMode::Free;
    std::string followTargetName_;
    float followDistance_ = 5.0f;    // 三人称: ターゲットからの距離
    float followHeight_ = 2.0f;      // 三人称: ターゲットからの高さ
    float followPitch_ = 15.0f;      // 三人称: 見下ろし角度（度）
    Vector3 firstPersonOffset_ = Vector3(0.0f, 1.7f, 0.0f);  // 一人称: 目の位置オフセット
    float followSmoothness_ = 10.0f; // 追従の滑らかさ
    float mouseSensitivity_ = 0.3f;  // 一人称: マウス感度
    float firstPersonMoveSpeed_ = 5.0f; // 一人称: 移動速度
    bool hideTargetInFirstPerson_ = true; // 一人称: ターゲットモデルを非表示にするか
    float cameraYaw_ = 0.0f;         // 一人称: 累積Yaw角度
    float cameraPitch_ = 0.0f;       // 一人称: 累積Pitch角度
    bool isPlaying_ = false;         // 再生中フラグ
    bool mouseLocked_ = false;       // マウスロック状態
    int mouseLockX_ = 0;             // マウスロック位置X
    int mouseLockY_ = 0;             // マウスロック位置Y
    class Scene* scene_ = nullptr;
};

} // namespace UnoEngine
