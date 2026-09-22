#pragma once

#include "Block/Block.h"

#include <string>

enum class PlantSupport {
    Dirt,
    DirtWithoutLiquid,
    DirtNetherrackSoulSand,
    DirtSandClay,
    Farmland,
    SoulSand,
    Mushroom,
    Reeds,
    Cactus
};

class PlantBlock : public Block {
public:
    explicit PlantBlock(const Block &block);

    static bool matches(const std::string &identifier);

    bool canBeReplaced(const BlockState &state) const override;

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool getDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                  std::vector<BlockDrop> &drops) const override;
};
