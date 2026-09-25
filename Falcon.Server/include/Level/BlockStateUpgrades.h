#pragma once

#include "Core/BlockState/BlockStateData.h"

#include <string>

class BlockStateUpgrades {
public:
    static BlockStateData upgrade(const BlockStateData &state);

    static std::string currentName(const std::string &name);
};
