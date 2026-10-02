#include "Block/BlockActorClassRegistry.h"

#include "Plugin/PluginRegistrationScope.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {
    struct Entry {
        BlockActorClassRegistry::Factory mFactory;
        const void *mOwner;
        bool mActive;
    };

    std::unordered_map<std::string, std::vector<Entry>> &entries() {
        static std::unordered_map<std::string, std::vector<Entry>> registered;
        return registered;
    }
}

BlockActorClassRegistry::Registration::Registration(const char *blockActorId, Factory factory) {
    entries()[blockActorId].push_back(Entry{factory, PluginRegistrationScope::getOwner(),
                                            PluginRegistrationScope::isActive()});
}

std::unique_ptr<BlockActor> BlockActorClassRegistry::create(const std::string &blockActorId) {
    const auto it = entries().find(blockActorId);
    if (it == entries().end())
        return nullptr;

    for (auto entry = it->second.rbegin(); entry != it->second.rend(); ++entry) {
        if (entry->mActive)
            return entry->mFactory();
    }

    return nullptr;
}

void BlockActorClassRegistry::activate(const void *owner) {
    for (auto &blockActorId: entries()) {
        for (Entry &entry: blockActorId.second) {
            if (entry.mOwner == owner)
                entry.mActive = true;
        }
    }
}

void BlockActorClassRegistry::remove(const void *owner) {
    if (owner == nullptr)
        return;

    for (auto &blockActorId: entries()) {
        std::vector<Entry> &registered = blockActorId.second;
        registered.erase(std::remove_if(registered.begin(), registered.end(), [owner](const Entry &entry) {
            return entry.mOwner == owner;
        }), registered.end());
    }
}
