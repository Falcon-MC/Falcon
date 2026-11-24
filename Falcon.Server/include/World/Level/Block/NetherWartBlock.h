#pragma once

#include "World/Level/Block/CropBlock.h"

class NetherWartBlock : public CropBlock {
public:
    using CropBlock::CropBlock;

    static bool matches(const std::string &identifier);

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortune,
                  std::vector<BlockDrop> &drops) const override;

protected:
    const char *getGrowthState() const override {
        return "age";
    }

    int getMaxGrowth() const override {
        return 3;
    }

    int getGrowthChance() const override {
        return 10;
    }

    bool needsLight() const override {
        return false;
    }
};
