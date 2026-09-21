#pragma once

#include "Block/Block.h"

class FrostedIceBlock : public Block {
public:
    using Block::Block;

    static constexpr const char *IDENTIFIER = "minecraft:frosted_ice";

    static bool matches(const std::string &identifier);

    static int32_t nextMeltDelay();

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

private:
    static int countFrostedNeighbours(Level &level, const Vector3i &position);

    static void slightlyMelt(Level &level, const Vector3i &position, const BlockState &state, bool source);
};
