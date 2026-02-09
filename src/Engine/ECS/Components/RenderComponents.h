#pragma once
#include "Mymath.h"
#include <memory>
#include <string>
#include "Object3d.h"
#include "AnimatedModel.h"
#include "Skybox.h"

namespace ECS {

struct MeshRendererComponent {
    std::unique_ptr<Object3d> object3d;
    bool visible = true;
};

struct AnimatedModelComponent {
    std::unique_ptr<AnimatedModel> animatedModel;
    std::string currentAnimationName;
    bool animationPaused = false;
    bool isBlending = false;
    float blendTimer = 0.0f;
    float blendDuration = 0.3f;
    float animationSpeed = 1.0f;
};

struct MaterialOverrideComponent {
    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 emissiveFactor{0.0f, 0.0f, 0.0f};
    bool isPBR = true;
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.8f;
};

struct EnvironmentMapComponent {
    bool enabled = false;
    float intensity = 0.3f;
};

struct SkyboxComponent {
    std::unique_ptr<Skybox> skybox;
    bool enabled = false;
};

} // namespace ECS
