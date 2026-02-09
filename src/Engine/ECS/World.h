#pragma once
#include "Entity.h"
#include "Archetype.h"
#include "ComponentRegistry.h"
#include "ComponentArray.h"
#include "System.h"
#include "Query.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <any>
#include <algorithm>
#include <string>

namespace ECS {

struct EntityRecord {
    Archetype* archetype = nullptr;
    uint32_t row = 0;
};

class World {
public:
    World() {
        // Reserve index 0 for NullEntity
        m_generations.push_back(0);
    }

    ~World() = default;

    // --- Entity Lifecycle ---

    Entity CreateEntity() {
        uint32_t index;
        if (!m_freeIndices.empty()) {
            index = m_freeIndices.back();
            m_freeIndices.pop_back();
        } else {
            index = static_cast<uint32_t>(m_generations.size());
            m_generations.push_back(0);
        }
        uint32_t gen = m_generations[index];
        Entity entity = Entity::Create(index, gen);
        m_entitySignatures[entity.id] = ComponentSignature{};
        return entity;
    }

    void DestroyEntity(Entity entity) {
        if (!IsAlive(entity)) return;

        auto it = m_entityRecords.find(entity.id);
        if (it != m_entityRecords.end()) {
            auto& record = it->second;
            Entity movedEntity = record.archetype->RemoveEntity(record.row);
            if (movedEntity.IsValid()) {
                // Update the moved entity's row
                m_entityRecords[movedEntity.id].row = record.row;
            }
            m_entityRecords.erase(it);
        }

        m_entitySignatures.erase(entity.id);
        uint32_t index = entity.GetIndex();
        m_generations[index]++;
        m_freeIndices.push_back(index);
    }

    bool IsAlive(Entity entity) const {
        uint32_t index = entity.GetIndex();
        if (index >= m_generations.size()) return false;
        return m_generations[index] == entity.GetGeneration()
            && m_entitySignatures.contains(entity.id);
    }

    // --- Component Operations ---

    template <typename T>
    void AddComponent(Entity entity, T&& component) {
        if (!IsAlive(entity)) return;

        auto& oldSig = m_entitySignatures[entity.id];
        ComponentTypeId typeId = GetComponentTypeId<T>();

        // Already has this component
        if (oldSig.test(typeId)) return;

        ComponentSignature newSig = oldSig;
        newSig.set(typeId);

        Archetype* newArch = FindOrCreateArchetype(newSig);
        newArch->RegisterComponentType<T>();

        auto recordIt = m_entityRecords.find(entity.id);
        if (recordIt != m_entityRecords.end()) {
            // Entity is in an existing archetype - move it
            auto& oldRecord = recordIt->second;
            Archetype* oldArch = oldRecord.archetype;
            uint32_t oldRow = oldRecord.row;

            // Ensure target archetype has arrays for all old component types
            for (auto& [tid, arr] : oldArch->GetArrays()) {
                newArch->EnsureArray(tid, arr.get());
            }

            // Move shared components from old archetype to new
            oldArch->MoveComponentsTo(oldRow, newArch);

            // Add the new entity entry
            uint32_t newRow = newArch->AddEntity(entity);

            // Add the new component
            newArch->PushComponent<T>(std::forward<T>(component));

            // Remove from old archetype (swap-and-pop)
            Entity movedEntity = oldArch->RemoveEntity(oldRow);
            if (movedEntity.IsValid()) {
                m_entityRecords[movedEntity.id].row = oldRow;
            }

            m_entityRecords[entity.id] = { newArch, newRow };
        } else {
            // Entity has no archetype yet
            uint32_t newRow = newArch->AddEntity(entity);
            newArch->PushComponent<T>(std::forward<T>(component));
            m_entityRecords[entity.id] = { newArch, newRow };
        }

        m_entitySignatures[entity.id] = newSig;
    }

    template <typename T>
    void RemoveComponent(Entity entity) {
        if (!IsAlive(entity)) return;

        auto& oldSig = m_entitySignatures[entity.id];
        ComponentTypeId typeId = GetComponentTypeId<T>();

        if (!oldSig.test(typeId)) return;

        ComponentSignature newSig = oldSig;
        newSig.reset(typeId);

        auto& oldRecord = m_entityRecords[entity.id];
        Archetype* oldArch = oldRecord.archetype;
        uint32_t oldRow = oldRecord.row;

        if (newSig.none()) {
            // No components left - just remove from archetype
            Entity movedEntity = oldArch->RemoveEntity(oldRow);
            if (movedEntity.IsValid()) {
                m_entityRecords[movedEntity.id].row = oldRow;
            }
            m_entityRecords.erase(entity.id);
        } else {
            Archetype* newArch = FindOrCreateArchetype(newSig);

            // Copy shared components (excluding the removed one)
            for (auto& [tid, arr] : oldArch->GetArrays()) {
                if (tid == typeId) continue;
                auto* targetArr = newArch->GetRawArray(tid);
                if (!targetArr) {
                    // Need to register this type in new archetype
                    // The array should already exist from FindOrCreateArchetype
                    continue;
                }
                arr->MoveElementTo(oldRow, targetArr);
            }

            uint32_t newRow = newArch->AddEntity(entity);

            Entity movedEntity = oldArch->RemoveEntity(oldRow);
            if (movedEntity.IsValid()) {
                m_entityRecords[movedEntity.id].row = oldRow;
            }

            m_entityRecords[entity.id] = { newArch, newRow };
        }

        m_entitySignatures[entity.id] = newSig;
    }

    template <typename T>
    T& GetComponent(Entity entity) {
        auto& record = m_entityRecords.at(entity.id);
        return record.archetype->GetComponent<T>(record.row);
    }

    template <typename T>
    const T& GetComponent(Entity entity) const {
        auto& record = m_entityRecords.at(entity.id);
        return record.archetype->GetComponent<T>(record.row);
    }

    template <typename T>
    bool HasComponent(Entity entity) const {
        if (!IsAlive(entity)) return false;
        auto it = m_entitySignatures.find(entity.id);
        if (it == m_entitySignatures.end()) return false;
        return it->second.test(GetComponentTypeId<T>());
    }

    // --- Queries ---

    // Iterate all entities with the given component types
    template <typename... Ts, typename Func>
    void ForEach(Func&& fn) {
        ComponentSignature required = BuildSignature<Ts...>();
        for (auto& archetype : m_archetypes) {
            if (archetype->MatchesSignature(required)) {
                QueryArchetype<Ts...>(*archetype, fn);
            }
        }
    }

    // Get all archetypes matching a signature (for batch processing in systems)
    std::vector<Archetype*> GetMatchingArchetypes(const ComponentSignature& required) {
        std::vector<Archetype*> result;
        for (auto& archetype : m_archetypes) {
            if (archetype->MatchesSignature(required)) {
                result.push_back(archetype.get());
            }
        }
        return result;
    }

    // Find single entity with a specific tag component (e.g., PlayerTag)
    template <typename TagT>
    Entity FindEntityWith() {
        ComponentSignature required;
        required.set(GetComponentTypeId<TagT>());
        for (auto& archetype : m_archetypes) {
            if (archetype->MatchesSignature(required) && archetype->EntityCount() > 0) {
                return archetype->GetEntity(0);
            }
        }
        return NullEntity;
    }

    // --- System Management ---

    void RegisterSystem(std::unique_ptr<ISystem> system, int32_t priority) {
        m_systems.push_back({ std::move(system), priority });
        std::sort(m_systems.begin(), m_systems.end(),
            [](const auto& a, const auto& b) { return a.priority < b.priority; });
    }

    void UpdateSystems(float deltaTime) {
        for (auto& entry : m_systems) {
            entry.system->Update(*this, deltaTime);
        }
    }

    void ClearSystems() {
        m_systems.clear();
    }

    // --- Global Resources ---
    // Non-entity singletons (DXCommon*, Camera*, Input*, LightManager*, etc.)

    template <typename T>
    void SetResource(T resource) {
        m_resources[GetComponentTypeId<T>()] = resource;
    }

    template <typename T>
    T& GetResource() {
        return std::any_cast<T&>(m_resources.at(GetComponentTypeId<T>()));
    }

    template <typename T>
    T* TryGetResource() {
        auto it = m_resources.find(GetComponentTypeId<T>());
        if (it == m_resources.end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    template <typename T>
    bool HasResource() const {
        return m_resources.contains(GetComponentTypeId<T>());
    }

    // --- Cleanup ---

    void DestroyAllEntities() {
        m_entityRecords.clear();
        m_entitySignatures.clear();
        m_archetypes.clear();
        m_freeIndices.clear();
        // Reset generations but keep index 0 reserved
        m_generations.clear();
        m_generations.push_back(0);
    }

    uint32_t GetEntityCount() const {
        return static_cast<uint32_t>(m_entitySignatures.size());
    }

private:
    Archetype* FindOrCreateArchetype(const ComponentSignature& signature) {
        // Linear search (archetypes are typically few)
        for (auto& archetype : m_archetypes) {
            if (archetype->GetSignature() == signature) {
                return archetype.get();
            }
        }
        auto newArch = std::make_unique<Archetype>(signature);
        // Pre-register component arrays for all bits set in signature
        // This is done lazily when components are actually added
        Archetype* ptr = newArch.get();
        m_archetypes.push_back(std::move(newArch));
        return ptr;
    }

    std::vector<std::unique_ptr<Archetype>> m_archetypes;
    std::unordered_map<uint64_t, EntityRecord> m_entityRecords;
    std::unordered_map<uint64_t, ComponentSignature> m_entitySignatures;
    std::vector<uint32_t> m_freeIndices;
    std::vector<uint32_t> m_generations;

    struct SystemEntry {
        std::unique_ptr<ISystem> system;
        int32_t priority = 0;
    };
    std::vector<SystemEntry> m_systems;

    std::unordered_map<ComponentTypeId, std::any> m_resources;
};

} // namespace ECS
