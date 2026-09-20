#include "Block/BlockActorClassRegistry.h"

#include <unordered_map>

namespace {
    std::unordered_map<std::string, BlockActorClassRegistry::Factory> &entries() {
        static std::unordered_map<std::string, BlockActorClassRegistry::Factory> registered;
        return registered;
    }
}

BlockActorClassRegistry::Registration::Registration(const char *blockActorId, Factory factory) {
    entries()[blockActorId] = factory;
}

std::unique_ptr<BlockActor> BlockActorClassRegistry::create(const std::string &blockActorId) {
    const auto it = entries().find(blockActorId);
    return it == entries().end() ? nullptr : it->second();
}
