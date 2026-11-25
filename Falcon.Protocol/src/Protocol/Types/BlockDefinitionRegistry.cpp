#include "Protocol/Types/BlockDefinitionRegistry.h"

#include "Protocol/Types/PlacedBlockAliases.h"

void BlockDefinitionRegistry::registerDefinition(std::shared_ptr<BlockDefinition> definition) {
    int runtimeId = definition->getRuntimeId();
    const std::string &identifier = definition->getIdentifier();
    mByIdentifier.try_emplace(identifier, definition);
    mByRuntimeId[runtimeId] = std::move(definition);
}

std::shared_ptr<BlockDefinition> BlockDefinitionRegistry::getDefinition(int runtimeId) const {
    auto it = mByRuntimeId.find(runtimeId);
    return it == mByRuntimeId.end() ? nullptr : it->second;
}

std::shared_ptr<BlockDefinition> BlockDefinitionRegistry::getDefinition(const std::string &identifier) const {
    auto it = mByIdentifier.find(identifier);
    if (it != mByIdentifier.end())
        return it->second;

    const std::string &placed = PlacedBlockAliases::resolve(identifier);
    if (placed.empty())
        return nullptr;

    auto aliased = mByIdentifier.find(placed);
    return aliased == mByIdentifier.end() ? nullptr : aliased->second;
}

bool BlockDefinitionRegistry::isRegistered(const std::shared_ptr<BlockDefinition> &definition) const {
    if (!definition) return false;
    auto it = mByRuntimeId.find(definition->getRuntimeId());
    return it != mByRuntimeId.end() && it->second == definition;
}
