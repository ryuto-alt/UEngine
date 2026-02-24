#pragma once
#include "IScene.h"
#include "AnimatedModel.h"
#include "Object3d.h"
#include <memory>

class Enemy2AnimTestScene : public IScene {
public:
    Enemy2AnimTestScene() = default;
    ~Enemy2AnimTestScene() override = default;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    std::unique_ptr<AnimatedModel> model_;
    std::unique_ptr<Object3d>      object_;

    float elapsedTime_        = 0.0f;
    float transitionDuration_ = 0.3f;
    float modelScale_         = 1.0f;
    float cameraSpeed_        = 5.0f;
};
