#include "Actor/ActorClassRegistry.h"

#include "Actor/ServerActor.h"

#include <unordered_map>

namespace {
    std::unordered_map<std::string, ActorClassRegistry::Factory> &entries() {
        static std::unordered_map<std::string, ActorClassRegistry::Factory> registered;
        return registered;
    }
}

ActorClassRegistry::Registration::Registration(const char *identifier, Factory factory) {
    entries()[identifier] = factory;
}

std::unique_ptr<ServerActor> ActorClassRegistry::create(uint64_t runtimeId, const std::string &identifier) {
    const auto it = entries().find(identifier);
    if (it == entries().end())
        return std::make_unique<ServerActor>(runtimeId, identifier);

    return it->second(runtimeId, identifier);
}
