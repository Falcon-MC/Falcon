#pragma once

#include "Block/Block.h"

#include <string>

class ObsidianBlock final : public Block {
public:
    explicit ObsidianBlock(const Block &block) : Block(block)
    {
    }

    static bool matches(const std::string &identifier);

    static bool tryLightPortal(Level &level, const Vector3i &firePosition, ServerNetworkHandler *owner);

private:
    static bool lightPortalAtBase(Level &level, const Vector3i &position, ServerNetworkHandler *owner);
};
