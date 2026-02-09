#pragma once
#include <cstdint>
#include <functional>

namespace ECS {

struct Entity {
    uint64_t id = 0;

    uint32_t GetIndex() const { return static_cast<uint32_t>(id & 0xFFFFFFFF); }
    uint32_t GetGeneration() const { return static_cast<uint32_t>(id >> 32); }

    static Entity Create(uint32_t index, uint32_t generation) {
        return Entity{ (static_cast<uint64_t>(generation) << 32) | index };
    }

    bool operator==(const Entity& other) const { return id == other.id; }
    bool operator!=(const Entity& other) const { return id != other.id; }
    bool IsValid() const { return id != 0; }
};

inline constexpr Entity NullEntity{0};

} // namespace ECS

template <>
struct std::hash<ECS::Entity> {
    size_t operator()(const ECS::Entity& e) const noexcept {
        return std::hash<uint64_t>{}(e.id);
    }
};
