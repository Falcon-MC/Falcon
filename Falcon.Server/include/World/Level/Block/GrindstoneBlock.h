#pragma once

#include "World/Level/Block/Block.h"

class GrindstoneBlock final : public Block {
public:
    explicit GrindstoneBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;
};
