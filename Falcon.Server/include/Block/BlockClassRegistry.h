#pragma once

#include "Block/Block.h"

#include <memory>
#include <string>

class BlockClassRegistry {
public:
    using Matcher = bool (*)(const std::string &);
    using Factory = std::unique_ptr<Block> (*)(const Block &);

    struct Registration {
        Registration(int priority, Matcher matcher, Factory factory);
    };

    static std::unique_ptr<Block> create(const Block &block);
};

#define FALCON_REGISTER_BLOCK(type, priority)                                            \
    static const BlockClassRegistry::Registration gBlockRegistration##type(              \
            (priority), &type::matches,                                                  \
            [](const Block &block) -> std::unique_ptr<Block> {                           \
                return std::make_unique<type>(block);                                    \
            })
