#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Feature/IFeature.h"

#include <cstdint>

class Level;

class NetherFeature : public IFeature {
public:
    static int64_t chunkHash(int32_t chunkX, int32_t chunkZ);

    static bool isAir(const BlockState &state);

    static bool isName(const BlockState &state, const char *identifier);

    static bool isSolid(const BlockState &state);

    static bool canBeReplaced(const BlockState &state);

    static int32_t getBiomeIdAt(Level &level, int32_t x, int32_t z);

protected:
    void seedForChunk(Level &level, int32_t chunkX, int32_t chunkZ);
};
