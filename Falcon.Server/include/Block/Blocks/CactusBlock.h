#pragma once

#include "Block/Blocks/PlantBlock.h"

class CactusBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    PathHazard getPathHazard() const override {
        return PathHazard::Damaging;
    }

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
