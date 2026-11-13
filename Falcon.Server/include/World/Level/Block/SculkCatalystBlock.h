#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class SculkCatalystBlock final : public Block {
public:
    explicit SculkCatalystBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;
};
