#pragma once

#include "Core/BlockState/BlockStateData.h"

class BlockStateUpgrades {
public:
    static BlockStateData upgrade(const BlockStateData &state);
};
