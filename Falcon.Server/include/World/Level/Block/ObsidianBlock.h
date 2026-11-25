#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class ObsidianBlock final : public Block {
public:
    explicit ObsidianBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    static bool tryLightPortal(Level &level, const Vector3i &firePosition, ServerNetworkHandler *owner);

    // stops the wither's charge, not the blocks it breaks when hurt
    bool resistsWither(bool charging) const override {
        return charging;
    }

private:
    static bool lightPortalAtBase(Level &level, const Vector3i &position, ServerNetworkHandler *owner);
};
