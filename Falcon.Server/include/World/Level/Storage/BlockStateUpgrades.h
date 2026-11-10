#pragma once

#include "Core/BlockState/BlockStateData.h"

#include <cstdint>
#include <string>

class BlockStateUpgrades {
public:
    static BlockStateData upgrade(const BlockStateData &state);

    static std::string currentName(const std::string &name);

    // a block saved as a numeric id and meta value, as worlds before 1.12 did, in the 1.12 form
    // upgrade() takes
    static BlockStateData fromLegacy(int32_t id, int32_t meta);
};
