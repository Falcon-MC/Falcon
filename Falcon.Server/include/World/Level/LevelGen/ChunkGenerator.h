#pragma once

#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"

#include <cstdint>
#include <vector>

class Level;
class LevelChunk;

class ChunkGenerator {
public:
    virtual ~ChunkGenerator() = default;

    virtual DimensionType getDimensionType() const = 0;

    virtual void generate(LevelChunk &chunk) const = 0;

    virtual void decorate(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) const = 0;

    virtual int64_t getSeed() const = 0;

    virtual int getSpawnY() const = 0;

    virtual int32_t getAirHash() const = 0;

    virtual int32_t pickBiome(int32_t x, int32_t y, int32_t z) const = 0;
};
