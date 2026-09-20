#pragma once

#include "Block/BlockActor.h"

#include <memory>
#include <string>

class BlockActorClassRegistry {
public:
    using Factory = std::unique_ptr<BlockActor> (*)();

    struct Registration {
        Registration(const char *blockActorId, Factory factory);
    };

    static std::unique_ptr<BlockActor> create(const std::string &blockActorId);
};

#define FALCON_REGISTER_BLOCK_ACTOR(type)                                                \
    static const BlockActorClassRegistry::Registration gBlockActorRegistration##type(    \
            type::BLOCK_ACTOR_ID,                                                        \
            []() -> std::unique_ptr<BlockActor> {                                        \
                return std::make_unique<type>();                                         \
            })
