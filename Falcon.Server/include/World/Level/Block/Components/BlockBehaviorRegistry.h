#pragma once

#include <string>

class BlockBehavior;

class BlockBehaviorRegistry {
public:
    static const BlockBehavior &get(const std::string &identifier);

    static void registerBehavior(const std::string &identifier, const BlockBehavior &behavior);
};
