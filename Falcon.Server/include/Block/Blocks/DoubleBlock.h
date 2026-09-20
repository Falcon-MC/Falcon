#pragma once

#include "Block/Blocks/PlantBlock.h"

class DoubleBlock {
public:
    static bool isUpperHalf(const BlockState &state);

    static std::vector<Vector3i> otherHalf(Level &level, const Vector3i &position, const BlockState &state);

    static bool isComplete(Level &level, const Vector3i &position, const BlockState &state);
};

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
};
