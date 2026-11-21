#pragma once

#include "World/Level/Block/SpeleothemBlock.h"

class IcicleBlock final : public SpeleothemBlock {
public:
    explicit IcicleBlock(const Block &block) : SpeleothemBlock(block) {}

    static constexpr const char *IDENTIFIER = "minecraft:icicle";

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
