#pragma once
#include <cstdint>
#include <bitset>

namespace ECS {

using ComponentTypeId = uint32_t;
inline constexpr uint32_t kMaxComponentTypes = 128;
using ComponentSignature = std::bitset<kMaxComponentTypes>;

namespace Internal {
    inline ComponentTypeId s_nextComponentId = 0;
}

template <typename T>
ComponentTypeId GetComponentTypeId() {
    static ComponentTypeId s_id = Internal::s_nextComponentId++;
    return s_id;
}

// Build a signature from a variadic list of component types
template <typename... Ts>
ComponentSignature BuildSignature() {
    ComponentSignature sig;
    (sig.set(GetComponentTypeId<Ts>()), ...);
    return sig;
}

} // namespace ECS
