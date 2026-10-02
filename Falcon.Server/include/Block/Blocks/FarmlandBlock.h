#pragma once

#include "Block/Block.h"

class FarmlandBlock : public Block {
public:
    explicit FarmlandBlock(const Block &block) : Block(block) {
    }

    static constexpr const char *IDENTIFIER = "minecraft:farmland";

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    void onFallOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                  const BlockState &state, float fallDistance) const override;

private:
    static bool isHydrated(Level &level, const Vector3i &position);

    static bool isNearWater(Level &level, const Vector3i &position);

    static bool isRainingAbove(Level &level, const Vector3i &position);

    static bool maintainsFarmland(Level &level, const Vector3i &position);

    static void turnToDirt(Level &level, const Vector3i &position);
};
