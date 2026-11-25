#pragma once

#include "Protocol/Types/ItemDefinition.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ItemDefinitionRegistry {
public:
    void registerDefinition(std::shared_ptr<ItemDefinition> definition);

    std::shared_ptr<ItemDefinition> getDefinition(int runtimeId) const;

    std::shared_ptr<ItemDefinition> getDefinition(const std::string &identifier) const;

    std::vector<std::shared_ptr<ItemDefinition>> getAll() const;

    size_t size() const { return mByRuntimeId.size(); }

private:
    std::vector<std::shared_ptr<ItemDefinition>> mOrdered;
    std::unordered_map<int, std::shared_ptr<ItemDefinition>> mByRuntimeId;
    std::unordered_map<std::string, std::shared_ptr<ItemDefinition>> mByIdentifier;
};
