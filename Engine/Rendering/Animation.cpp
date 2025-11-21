#include "Animation.h"
#include <algorithm>

namespace UnoEngine {

float Animation::GetDuration() const {
    float maxTime = 0.0f;
    for (const auto& sampler : samplers_) {
        if (!sampler.times.empty()) {
            maxTime = std::max(maxTime, sampler.times.back());
        }
    }
    return maxTime;
}

} // namespace UnoEngine
