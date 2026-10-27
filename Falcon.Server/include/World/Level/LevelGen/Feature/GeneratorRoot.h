#pragma once

#include "World/Level/LevelGen/Feature/BlockManager.h"

class GeneratorRoot : public BlockManager {
public:
    explicit GeneratorRoot(Level &level);

    bool canReplace(int32_t x, int32_t y, int32_t z) const;

    void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override;
};
