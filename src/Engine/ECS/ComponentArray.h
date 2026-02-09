#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <utility>

namespace ECS {

class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void SwapRemoveAt(uint32_t index) = 0;
    virtual uint32_t Size() const = 0;
    virtual std::unique_ptr<IComponentArray> CreateEmpty() const = 0;
    virtual void MoveElementTo(uint32_t index, IComponentArray* target) = 0;
};

template <typename T>
class ComponentArray final : public IComponentArray {
public:
    void Add(const T& component) {
        m_data.push_back(component);
    }

    void Add(T&& component) {
        m_data.push_back(std::move(component));
    }

    T& Get(uint32_t index) {
        return m_data[index];
    }

    const T& Get(uint32_t index) const {
        return m_data[index];
    }

    void SwapRemoveAt(uint32_t index) override {
        if (index < m_data.size() - 1) {
            m_data[index] = std::move(m_data.back());
        }
        m_data.pop_back();
    }

    uint32_t Size() const override {
        return static_cast<uint32_t>(m_data.size());
    }

    std::unique_ptr<IComponentArray> CreateEmpty() const override {
        return std::make_unique<ComponentArray<T>>();
    }

    void MoveElementTo(uint32_t index, IComponentArray* target) override {
        auto* typedTarget = static_cast<ComponentArray<T>*>(target);
        typedTarget->Add(std::move(m_data[index]));
    }

    T* Data() { return m_data.data(); }
    const T* Data() const { return m_data.data(); }

    std::vector<T>& GetVector() { return m_data; }
    const std::vector<T>& GetVector() const { return m_data; }

private:
    std::vector<T> m_data;
};

} // namespace ECS
