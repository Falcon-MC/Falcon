#pragma once

#include "Block/Blocks/SpeleothemBlock.h"

class PointedDripstoneBlock : public SpeleothemBlock {
public:
    explicit PointedDripstoneBlock(const Block &block) : SpeleothemBlock(block) {
    }

    static constexpr const char *IDENTIFIER = "minecraft:pointed_dripstone";

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    void growStalagmiteBelow(Level &level, const Vector3i &tip) const;
};
