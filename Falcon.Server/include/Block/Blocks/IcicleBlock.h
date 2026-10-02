#pragma once

#include "Block/Blocks/SpeleothemBlock.h"

class IcicleBlock : public SpeleothemBlock {
public:
    explicit IcicleBlock(const Block &block) : SpeleothemBlock(block) {
    }

    static constexpr const char *IDENTIFIER = "minecraft:icicle";

    static bool matches(const std::string &identifier);

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
