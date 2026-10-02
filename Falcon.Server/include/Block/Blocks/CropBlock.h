#pragma once

#include "Block/Blocks/PlantBlock.h"

class CropBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

protected:
    virtual const char *getGrowthState() const { return "growth"; }

    virtual int getMaxGrowth() const { return 7; }

    virtual int getGrowthChance() const { return 2; }

    virtual bool needsLight() const { return true; }
};
