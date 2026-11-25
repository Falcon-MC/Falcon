#pragma once

#include "World/Level/Block/PlantBlock.h"

class CropBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortune,
                  std::vector<BlockDrop> &drops) const override;

    virtual bool growBy(Level &level, const Vector3i &position, const BlockState &state, int amount,
                        ServerPlayer *player) const;

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }

protected:
    static int rollSeeds(int growth, int rolls);

    virtual const char *getGrowthState() const {
        return "growth";
    }

    virtual int getMaxGrowth() const {
        return 7;
    }

    virtual int getGrowthChance() const {
        return 2;
    }

    virtual bool needsLight() const {
        return true;
    }
};
