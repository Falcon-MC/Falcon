#pragma once

#include <cstdint>
#include <memory>
#include <string>

class ServerActor;

class ActorClassRegistry {
public:
    using Factory = std::unique_ptr<ServerActor> (*)(uint64_t, const std::string &);

    struct Registration {
        Registration(const char *identifier, Factory factory);
    };

    static std::unique_ptr<ServerActor> create(uint64_t runtimeId, const std::string &identifier);
};

#define FALCON_REGISTER_ACTOR(type, identifier)                                          \
    static const ActorClassRegistry::Registration gActorRegistration##type(              \
            (identifier),                                                                \
            [](uint64_t runtimeId, const std::string &actorIdentifier)                   \
                    -> std::unique_ptr<ServerActor> {                                    \
                return std::make_unique<type>(runtimeId, actorIdentifier);               \
            })
