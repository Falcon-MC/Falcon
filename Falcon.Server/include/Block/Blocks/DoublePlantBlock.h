#pragma once

#include "Block/Blocks/PlantBlock.h"

class DoublePlantBlock : public PlantBlock {
public:
    explicit DoublePlantBlock(const Block &block) : PlantBlock(block) {}

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    std::vector<BlockPlacementEntry> getPlacementBlocks(Level &level, const Vector3i &position,
                                                        const BlockState &state,
                                                        int playerFacing) const override;

    std::vector<Vector3i> getAffectedBlocks(Level &level, const Vector3i &position,
                                            const BlockState &state) const override;

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;
};
