#pragma once

#include "World/Level/Block/PlantBlock.h"
#include "World/Level/LevelGen/Feature/Tree/TreeWoodType.h"

class SaplingBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }

private:
    bool growTree(ServerNetworkHandler &owner, Level &level, const Vector3i &position) const;

    TreeWoodType getWoodType() const;
};
