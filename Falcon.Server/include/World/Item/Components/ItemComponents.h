#pragma once

#include "World/Item/Components/ItemComponentTypes.h"

#include <cstddef>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>

class ItemComponents {
public:
    ItemComponents() = default;

    ItemComponents(const ItemComponents &other) {
        for (const auto &entry: other.mComponents)
            mComponents.emplace(entry.first, entry.second->clone());
    }

    ItemComponents &operator=(const ItemComponents &other) {
        if (this == &other)
            return *this;

        ItemComponents copy(other);
        mComponents.swap(copy.mComponents);
        return *this;
    }

    ItemComponents(ItemComponents &&) noexcept = default;
    ItemComponents &operator=(ItemComponents &&) noexcept = default;

    template <typename T, typename... Args>
    T &emplace(Args &&... args) {
        static_assert(std::is_base_of_v<ItemComponent, T>);
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T &result = *component;
        mComponents[std::type_index(typeid(T))] = std::move(component);
        return result;
    }

    template <typename T>
    T *get() {
        const auto it = mComponents.find(std::type_index(typeid(T)));
        return it == mComponents.end() ? nullptr : static_cast<T *>(it->second.get());
    }

    template <typename T>
    const T *get() const {
        const auto it = mComponents.find(std::type_index(typeid(T)));
        return it == mComponents.end() ? nullptr : static_cast<const T *>(it->second.get());
    }

    template <typename T>
    bool has() const {
        return mComponents.find(std::type_index(typeid(T))) != mComponents.end();
    }

    bool empty() const { return mComponents.empty(); }
    size_t size() const { return mComponents.size(); }

private:
    std::unordered_map<std::type_index, std::unique_ptr<ItemComponent>> mComponents;
};
