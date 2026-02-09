#pragma once

namespace ECS {

class World;

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(World& world, float deltaTime) = 0;
    virtual const char* GetName() const = 0;
};

} // namespace ECS
