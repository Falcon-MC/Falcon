#pragma once

#include "World/Level/Block/CropBlock.h"

class StemBlock : public CropBlock {
public:
    using CropBlock::CropBlock;

    static bool matches(const std::string &identifier);

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortune,
                  std::vector<BlockDrop> &drops) const override;

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

private:
    BlockState getFruitState() const;
};
