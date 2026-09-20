#include "Actor/ActorClassRegistry.h"

#include "Actor/Mob/MobActor.h"
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

const ServerActor *ActorClassRegistry::getPrototype(const std::string &identifier) {
    static std::unordered_map<std::string, std::unique_ptr<ServerActor>> prototypes;

    const auto known = prototypes.find(identifier);
    if (known != prototypes.end())
        return known->second.get();

    const auto entry = entries().find(identifier);
    if (entry == entries().end())
        return nullptr;

    std::unique_ptr<ServerActor> prototype = entry->second(0, identifier);
    const ServerActor *result = prototype.get();
    prototypes[identifier] = std::move(prototype);

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
    const auto it = entries().find(identifier);
    if (it == entries().end())
        return std::make_unique<ServerActor>(runtimeId, identifier);

    return it->second(runtimeId, identifier);
}
