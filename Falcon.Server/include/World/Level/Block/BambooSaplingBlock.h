#pragma once

#include "World/Level/Block/PlantBlock.h"

class BambooSaplingBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;
};
