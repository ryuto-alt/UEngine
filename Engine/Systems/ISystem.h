#pragma once

namespace UnoEngine {

class Scene;

class ISystem {
public:
    virtual ~ISystem() = default;
    
    virtual void OnSceneStart(Scene* scene) {}
    virtual void OnUpdate(Scene* scene, float deltaTime) = 0;
    virtual void OnSceneEnd(Scene* scene) {}
    
    virtual int GetPriority() const { return 100; }
    
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

private:
    bool enabled_ = true;
};

} // namespace UnoEngine
