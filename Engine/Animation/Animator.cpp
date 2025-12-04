#include "pch.h"
#include "Animator.h"

namespace UnoEngine {

void Animator::OnUpdate(float deltaTime) {
    if (!isPlaying_ || !skeleton_) {
        return;
    }

    if (isTransitioning_ && nextState_) {
        transitionTime_ += deltaTime;
        float blendFactor = transitionTime_ / transitionDuration_;

        if (blendFactor >= 1.0f) {
            currentState_ = nextState_;
            nextState_ = nullptr;
            isTransitioning_ = false;
            transitionTime_ = 0.0f;
        } else {
            if (currentState_) {
                currentState_->Update(deltaTime);
            }
            nextState_->Update(deltaTime);
            BlendAnimations(blendFactor);
            return;
        }
    }

    if (currentState_) {
        currentState_->Update(deltaTime);
        CheckTransitions();
    }

    UpdateBoneMatrices();
}

void Animator::SetSkeleton(std::shared_ptr<Skeleton> skeleton) {
    skeleton_ = skeleton;
    if (skeleton_) {
        uint32 boneCount = skeleton_->GetBoneCount();
        finalBoneMatrices_.resize(boneCount);
        finalBoneMatrixPairs_.resize(boneCount);
        currentLocalTransforms_.resize(boneCount);
        nextLocalTransforms_.resize(boneCount);

        // バインドポーズを初期状態として設定（アニメーション前のレンダリングで倒れないように）
        skeleton_->ComputeBindPoseMatrices(finalBoneMatrices_);

        // ローカル変換もバインドポーズで初期化
        const auto& bones = skeleton_->GetBones();
        for (uint32 i = 0; i < boneCount; ++i) {
            currentLocalTransforms_[i] = bones[i].localBindPose;
            nextLocalTransforms_[i] = bones[i].localBindPose;
        }

        // BoneMatrixPairsもバインドポーズで初期化（アニメーション再生前の描画で爆発しないように）
        skeleton_->ComputeBoneMatricesWithInverseTranspose(currentLocalTransforms_, finalBoneMatrixPairs_);
    }
}

void Animator::AddClip(const std::string& name, std::shared_ptr<AnimationClip> clip) {
    clips_[name] = clip;
}

AnimationClip* Animator::GetClip(const std::string& name) const {
    auto it = clips_.find(name);
    if (it != clips_.end()) {
        return it->second.get();
    }
    return nullptr;
}

AnimationState* Animator::AddState(const std::string& stateName, const std::string& clipName) {
    auto clipIt = clips_.find(clipName);
    if (clipIt == clips_.end()) {
        return nullptr;
    }

    auto state = std::make_unique<AnimationState>(stateName, clipIt->second);
    AnimationState* ptr = state.get();
    states_[stateName] = std::move(state);
    return ptr;
}

AnimationState* Animator::GetState(const std::string& stateName) {
    auto it = states_.find(stateName);
    if (it != states_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void Animator::Play(const std::string& stateName, float transitionDuration) {
    AnimationState* state = GetState(stateName);
    if (!state) {
        return;
    }

    if (transitionDuration > 0.0f && currentState_) {
        CrossFade(stateName, transitionDuration);
    } else {
        state->Reset();
        currentState_ = state;
        isPlaying_ = true;
        isTransitioning_ = false;

        // 初期フレームを即座に適用（最初のレンダリング前に正しいポーズにする）
        UpdateBoneMatrices();
    }
}

void Animator::CrossFade(const std::string& stateName, float duration) {
    AnimationState* state = GetState(stateName);
    if (!state || state == currentState_) {
        return;
    }

    state->Reset();
    nextState_ = state;
    transitionDuration_ = duration;
    transitionTime_ = 0.0f;
    isTransitioning_ = true;
    isPlaying_ = true;
}

void Animator::Stop() {
    isPlaying_ = false;
    isTransitioning_ = false;
    currentState_ = nullptr;
    nextState_ = nullptr;
}

float Animator::GetCurrentTime() const {
    if (currentState_) {
        return currentState_->GetCurrentTime();
    }
    return 0.0f;
}

float Animator::GetNormalizedTime() const {
    if (currentState_) {
        return currentState_->GetNormalizedTime();
    }
    return 0.0f;
}

void Animator::SetParameter(const std::string& name, float value) {
    floatParams_[name] = value;
}

void Animator::SetParameter(const std::string& name, int32 value) {
    intParams_[name] = value;
}

void Animator::SetParameter(const std::string& name, bool value) {
    boolParams_[name] = value;
}

float Animator::GetFloatParameter(const std::string& name) const {
    auto it = floatParams_.find(name);
    return it != floatParams_.end() ? it->second : 0.0f;
}

int32 Animator::GetIntParameter(const std::string& name) const {
    auto it = intParams_.find(name);
    return it != intParams_.end() ? it->second : 0;
}

bool Animator::GetBoolParameter(const std::string& name) const {
    auto it = boolParams_.find(name);
    return it != boolParams_.end() ? it->second : false;
}

void Animator::UpdateBoneMatrices() {
    if (!skeleton_ || !currentState_ || !currentState_->GetClip()) {
        return;
    }

    float time = currentState_->GetCurrentTime();
    currentState_->GetClip()->Sample(time, *skeleton_, currentLocalTransforms_);
    
    // 新しいメソッドを使用してInverseTranspose行列も計算
    skeleton_->ComputeBoneMatricesWithInverseTranspose(currentLocalTransforms_, finalBoneMatrixPairs_);
    
    // 後方互換性のため、従来のmatrix配列も更新
    skeleton_->ComputeBoneMatrices(currentLocalTransforms_, finalBoneMatrices_);

}

void Animator::CheckTransitions() {
    if (!currentState_ || isTransitioning_) {
        return;
    }

    const auto& transitions = currentState_->GetTransitions();
    for (const auto& transition : transitions) {
        if (transition.condition && transition.condition()) {
            CrossFade(transition.targetStateName, transition.duration);
            break;
        }
    }
}

void Animator::BlendAnimations(float blendFactor) {
    if (!skeleton_ || !currentState_ || !nextState_) {
        return;
    }

    AnimationClip* currentClip = currentState_->GetClip();
    AnimationClip* nextClip = nextState_->GetClip();

    if (!currentClip || !nextClip) {
        return;
    }

    currentClip->Sample(currentState_->GetCurrentTime(), *skeleton_, currentLocalTransforms_);
    nextClip->Sample(nextState_->GetCurrentTime(), *skeleton_, nextLocalTransforms_);

    uint32 boneCount = skeleton_->GetBoneCount();
    std::vector<Matrix4x4> blendedTransforms(boneCount);

    for (uint32 i = 0; i < boneCount; ++i) {
        blendedTransforms[i] = Matrix4x4::Lerp(currentLocalTransforms_[i], nextLocalTransforms_[i], blendFactor);
    }

    skeleton_->ComputeBoneMatrices(blendedTransforms, finalBoneMatrices_);
}

} // namespace UnoEngine
