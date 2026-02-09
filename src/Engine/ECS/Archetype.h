#pragma once
#include "Entity.h"
#include "ComponentRegistry.h"
#include "ComponentArray.h"
#include <unordered_map>
#include <vector>
#include <memory>

namespace ECS {

class Archetype {
public:
    explicit Archetype(ComponentSignature signature)
        : m_signature(signature) {}

    const ComponentSignature& GetSignature() const { return m_signature; }
    uint32_t EntityCount() const { return static_cast<uint32_t>(m_entities.size()); }
    const std::vector<Entity>& GetEntities() const { return m_entities; }
    Entity GetEntity(uint32_t index) const { return m_entities[index]; }

    template <typename T>
    void RegisterComponentType() {
        auto typeId = GetComponentTypeId<T>();
        if (!m_arrays.contains(typeId)) {
            m_arrays[typeId] = std::make_unique<ComponentArray<T>>();
        }
    }

    // Returns the row index of the newly added entity
    uint32_t AddEntity(Entity entity) {
        uint32_t row = static_cast<uint32_t>(m_entities.size());
        m_entities.push_back(entity);
        return row;
    }

    template <typename T>
    void PushComponent(const T& component) {
        auto typeId = GetComponentTypeId<T>();
        static_cast<ComponentArray<T>*>(m_arrays.at(typeId).get())->Add(component);
    }

    template <typename T>
    void PushComponent(T&& component) {
        auto typeId = GetComponentTypeId<T>();
        static_cast<ComponentArray<T>*>(m_arrays.at(typeId).get())->Add(std::move(component));
    }

    template <typename T>
    T& GetComponent(uint32_t row) {
        auto typeId = GetComponentTypeId<T>();
        return static_cast<ComponentArray<T>*>(m_arrays.at(typeId).get())->Get(row);
    }

    template <typename T>
    const T& GetComponent(uint32_t row) const {
        auto typeId = GetComponentTypeId<T>();
        return static_cast<const ComponentArray<T>*>(m_arrays.at(typeId).get())->Get(row);
    }

    template <typename T>
    ComponentArray<T>* GetComponentArray() {
        auto typeId = GetComponentTypeId<T>();
        auto it = m_arrays.find(typeId);
        if (it == m_arrays.end()) return nullptr;
        return static_cast<ComponentArray<T>*>(it->second.get());
    }

    template <typename T>
    bool HasComponentType() const {
        return m_signature.test(GetComponentTypeId<T>());
    }

    bool MatchesSignature(const ComponentSignature& required) const {
        return (m_signature & required) == required;
    }

    IComponentArray* GetRawArray(ComponentTypeId typeId) {
        auto it = m_arrays.find(typeId);
        return it != m_arrays.end() ? it->second.get() : nullptr;
    }

    // Ensure array exists for a given type by cloning from source
    void EnsureArray(ComponentTypeId typeId, IComponentArray* source) {
        if (!m_arrays.contains(typeId)) {
            m_arrays[typeId] = source->CreateEmpty();
        }
    }

    const std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentArray>>& GetArrays() const {
        return m_arrays;
    }

    // Swap-remove entity at row, returns the entity that was moved into the gap (or NullEntity if last)
    Entity RemoveEntity(uint32_t row) {
        Entity movedEntity = NullEntity;
        uint32_t lastIndex = static_cast<uint32_t>(m_entities.size()) - 1;

        if (row < lastIndex) {
            movedEntity = m_entities[lastIndex];
            m_entities[row] = m_entities[lastIndex];
            for (auto& [typeId, arr] : m_arrays) {
                arr->SwapRemoveAt(row);
            }
        } else {
            for (auto& [typeId, arr] : m_arrays) {
                arr->SwapRemoveAt(row);
            }
        }
        m_entities.pop_back();
        return movedEntity;
    }

    // Move all component data for a row to another archetype (for shared component types)
    void MoveComponentsTo(uint32_t row, Archetype* target) {
        for (auto& [typeId, arr] : m_arrays) {
            auto* targetArr = target->GetRawArray(typeId);
            if (targetArr) {
                arr->MoveElementTo(row, targetArr);
            }
        }
    }

private:
    ComponentSignature m_signature;
    std::vector<Entity> m_entities;
    std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentArray>> m_arrays;
};

} // namespace ECS
