#include "Actor/ActorClassRegistry.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerActor.h"
#include "Plugin/PluginRegistrationScope.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {
    struct Entry {
        ActorClassRegistry::Factory mFactory;
        const void *mOwner;
        bool mActive;
    };

    std::unordered_map<std::string, std::vector<Entry>> &entries() {
        static std::unordered_map<std::string, std::vector<Entry>> registered;
        return registered;
    }

    std::unordered_map<std::string, std::unique_ptr<ServerActor>> &prototypes() {
        static std::unordered_map<std::string, std::unique_ptr<ServerActor>> cached;
        return cached;
    }

    std::vector<std::unique_ptr<ServerActor>> &retiredPrototypes() {
        static auto *retired = new std::vector<std::unique_ptr<ServerActor>>();
        return *retired;
    }

    ActorClassRegistry::Factory findFactory(const std::string &identifier) {
        const auto it = entries().find(identifier);
        if (it == entries().end())
            return nullptr;

        for (auto entry = it->second.rbegin(); entry != it->second.rend(); ++entry) {
            if (entry->mActive)
                return entry->mFactory;
        }

        return nullptr;
    }
}

ActorClassRegistry::Registration::Registration(const char *identifier, Factory factory) {
    entries()[identifier].push_back(Entry{factory, PluginRegistrationScope::getOwner(),
                                          PluginRegistrationScope::isActive()});
}

const ServerActor *ActorClassRegistry::getPrototype(const std::string &identifier) {
    std::unordered_map<std::string, std::unique_ptr<ServerActor>> &cached = prototypes();

    const auto known = cached.find(identifier);
    if (known != cached.end())
        return known->second.get();

    const Factory factory = findFactory(identifier);
    if (factory == nullptr)
        return nullptr;

    std::unique_ptr<ServerActor> prototype = factory(0, identifier);
    const ServerActor *result = prototype.get();
    cached[identifier] = std::move(prototype);

    return result;
}

const MobActor *ActorClassRegistry::getMobPrototype(const std::string &identifier) {
    return dynamic_cast<const MobActor *>(getPrototype(identifier));
}

ActorSize ActorClassRegistry::getSize(const std::string &identifier) {
    const ServerActor *prototype = getPrototype(identifier);
    if (prototype == nullptr)
        return ActorSize{ServerActor::DEFAULT_WIDTH, ServerActor::DEFAULT_HEIGHT};

    return prototype->getSize();
}

std::unique_ptr<ServerActor> ActorClassRegistry::create(uint64_t runtimeId, const std::string &identifier) {
    const Factory factory = findFactory(identifier);
    if (factory == nullptr)
        return std::make_unique<ServerActor>(runtimeId, identifier);

    return factory(runtimeId, identifier);
}

void ActorClassRegistry::activate(const void *owner) {
    for (auto &identifier: entries()) {
        for (Entry &entry: identifier.second) {
            if (entry.mOwner == owner)
                entry.mActive = true;
        }
    }
}

void ActorClassRegistry::remove(const void *owner) {
    if (owner == nullptr)
        return;

    for (auto &identifier: entries()) {
        std::vector<Entry> &registered = identifier.second;
        registered.erase(std::remove_if(registered.begin(), registered.end(), [owner](const Entry &entry) {
            return entry.mOwner == owner;
        }), registered.end());
    }
}

void ActorClassRegistry::resetPrototypes() {
    std::unordered_map<std::string, std::unique_ptr<ServerActor>> &cached = prototypes();
    for (auto &entry: cached)
        retiredPrototypes().push_back(std::move(entry.second));

    cached.clear();
}
