#pragma once
#include "ComponentRegistry.h"
#include "Archetype.h"
#include <functional>
#include <tuple>

namespace ECS {

// Invoke callback with typed component references for each matching entity
template <typename... Ts, typename Func>
void QueryArchetype(Archetype& archetype, Func&& fn) {
    uint32_t count = archetype.EntityCount();
    if (count == 0) return;

    // Get all component arrays upfront for cache efficiency
    auto arrays = std::make_tuple(archetype.GetComponentArray<Ts>()...);

    for (uint32_t i = 0; i < count; ++i) {
        Entity entity = archetype.GetEntity(i);
        fn(entity, std::get<ComponentArray<Ts>*>(arrays)->Get(i)...);
    }
}

// Read-only query variant
template <typename... Ts, typename Func>
void QueryArchetypeReadOnly(const Archetype& archetype, Func&& fn) {
    uint32_t count = archetype.EntityCount();
    if (count == 0) return;

    auto arrays = std::make_tuple(
        const_cast<Archetype&>(archetype).GetComponentArray<Ts>()...
    );

    for (uint32_t i = 0; i < count; ++i) {
        Entity entity = archetype.GetEntity(i);
        fn(entity, static_cast<const Ts&>(std::get<ComponentArray<Ts>*>(arrays)->Get(i))...);
    }
}

} // namespace ECS
