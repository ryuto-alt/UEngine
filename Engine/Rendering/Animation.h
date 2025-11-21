#pragma once

#include "../Math/Math.h"
#include "../Core/Types.h"
#include <vector>
#include <string>

namespace UnoEngine {

enum class AnimationPath {
    Translation,
    Rotation,
    Scale
};

enum class InterpolationType {
    Linear,
    Step,
    CubicSpline
};

struct AnimationSampler {
    InterpolationType interpolation = InterpolationType::Linear;
    std::vector<float> times;
    std::vector<Vector3> outputVec3;
    std::vector<Quaternion> outputQuat;

    float GetStartTime() const { return times.empty() ? 0.0f : times.front(); }
    float GetEndTime() const { return times.empty() ? 0.0f : times.back(); }
};

struct AnimationChannel {
    int32 samplerIndex = -1;
    int32 targetJointIndex = -1;
    AnimationPath path = AnimationPath::Translation;
};

class Animation {
public:
    Animation() = default;
    ~Animation() = default;

    void SetName(const std::string& name) { name_ = name; }
    const std::string& GetName() const { return name_; }

    void AddSampler(const AnimationSampler& sampler) { samplers_.push_back(sampler); }
    void AddChannel(const AnimationChannel& channel) { channels_.push_back(channel); }

    const std::vector<AnimationSampler>& GetSamplers() const { return samplers_; }
    const std::vector<AnimationChannel>& GetChannels() const { return channels_; }

    float GetDuration() const;

private:
    std::string name_;
    std::vector<AnimationSampler> samplers_;
    std::vector<AnimationChannel> channels_;
};

} // namespace UnoEngine
