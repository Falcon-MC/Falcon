#pragma once

#include "Block/Blocks/CropBlock.h"

class StemBlock : public CropBlock {
public:
    using CropBlock::CropBlock;

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

private:
    BlockState getFruitState() const;
};
