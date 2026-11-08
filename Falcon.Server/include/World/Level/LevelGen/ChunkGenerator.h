#pragma once

#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Chunk/GeneratedBlockChange.h"

#include <cstdint>
#include <memory>
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

    // y above the top solid or liquid block from terrain generation alone: what the game reads for an unloaded
    // column (sub_14A2298E0); generates a throwaway chunk: any thread, but not cheap
    virtual int32_t getPreliminaryHeight(int32_t x, int32_t z) const;

    // y above the top solid block of the generated column, carvers included and liquids skipped: where the game
    // lays a structure on the ground; generates a throwaway chunk
    int32_t getGeneratedSolidHeight(int32_t x, int32_t z) const;

    // a throwaway chunk of terrain, for reading many columns of the same chunk
    std::unique_ptr<LevelChunk> generateTerrain(int32_t chunkX, int32_t chunkZ) const;

    // getGeneratedSolidHeight for a chunk the caller already holds
    static int32_t solidHeight(const LevelChunk &chunk, int32_t localX, int32_t localZ);

    // the coarse height structures read their biomes at: the terrain density alone, with no water and cut to the
    // eight block noise cells, so a sea column reports its floor
    virtual int32_t getPreliminarySurfaceLevel(int32_t quartX, int32_t quartZ) const;

    // the lowest getPreliminaryHeight of the chunk's columns, from a single throwaway chunk
    virtual int32_t getPreliminaryLowestHeight(int32_t chunkX, int32_t chunkZ) const;
};
