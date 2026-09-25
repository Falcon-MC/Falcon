#pragma once

#include "Actor/ActorSize.h"

#include <cstdint>
#include <memory>
#include <string>

class MobActor;
class ServerActor;

class ActorClassRegistry {
public:
    using Factory = std::unique_ptr<ServerActor> (*)(uint64_t, const std::string &);

    struct Registration {
        Registration(const char *identifier, Factory factory);
    };

    static std::unique_ptr<ServerActor> create(uint64_t runtimeId, const std::string &identifier);

    static const ServerActor *getPrototype(const std::string &identifier);

    static const MobActor *getMobPrototype(const std::string &identifier);

    static ActorSize getSize(const std::string &identifier);

    static void activate(const void *owner);

    static void remove(const void *owner);

    static void resetPrototypes();
};

#define FALCON_REGISTER_SIZED_ACTOR(tag, identifier, width, height, projectile)          \
    static const ActorClassRegistry::Registration gActorRegistration##tag(               \
            (identifier),                                                                \
            [](uint64_t runtimeId, const std::string &actorIdentifier)                   \
                    -> std::unique_ptr<ServerActor> {                                    \
                return std::make_unique<SizedActor>(runtimeId, actorIdentifier,          \
                                                    ActorSize{width, height}, projectile); \
            })

#define FALCON_REGISTER_ACTOR(type, identifier)                                          \
    static const ActorClassRegistry::Registration gActorRegistration##type(              \
            (identifier),                                                                \
            [](uint64_t runtimeId, const std::string &actorIdentifier)                   \
                    -> std::unique_ptr<ServerActor> {                                    \
                return std::make_unique<type>(runtimeId, actorIdentifier);               \
            })
