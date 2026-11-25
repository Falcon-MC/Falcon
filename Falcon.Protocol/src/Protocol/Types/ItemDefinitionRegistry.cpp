#include "Protocol/Types/ItemDefinitionRegistry.h"

void ItemDefinitionRegistry::registerDefinition(std::shared_ptr<ItemDefinition> definition) {
    int runtimeId = definition->getRuntimeId();
    const std::string &identifier = definition->getIdentifier();
    mOrdered.push_back(definition);
    mByIdentifier.try_emplace(identifier, definition);
    mByRuntimeId[runtimeId] = std::move(definition);
}

std::shared_ptr<ItemDefinition> ItemDefinitionRegistry::getDefinition(int runtimeId) const {
    auto it = mByRuntimeId.find(runtimeId);
    return it == mByRuntimeId.end() ? nullptr : it->second;
}

std::shared_ptr<ItemDefinition> ItemDefinitionRegistry::getDefinition(const std::string &identifier) const {
    auto it = mByIdentifier.find(identifier);
    return it == mByIdentifier.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<ItemDefinition>> ItemDefinitionRegistry::getAll() const {
    return mOrdered;
}
