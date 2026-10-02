#pragma once

#include "Block/Blocks/PlantBlock.h"
#include "Level/Generator/Feature/Tree/TreeWoodType.h"

class SaplingBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    bool growTree(ServerNetworkHandler &owner, Level &level, const Vector3i &position) const;

    TreeWoodType getWoodType() const;
};
