#pragma once

#include "Protocol/Types/BlockDefinition.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class BlockDefinitionRegistry {
public:
    void registerDefinition(std::shared_ptr<BlockDefinition> definition);

    std::shared_ptr<BlockDefinition> getDefinition(int runtimeId) const;

    std::shared_ptr<BlockDefinition> getDefinition(const std::string &identifier) const;

    bool isRegistered(const std::shared_ptr<BlockDefinition> &definition) const;

    size_t size() const { return mByRuntimeId.size(); }

private:
    std::unordered_map<int, std::shared_ptr<BlockDefinition>> mByRuntimeId;
    std::unordered_map<std::string, std::shared_ptr<BlockDefinition>> mByIdentifier;
};
