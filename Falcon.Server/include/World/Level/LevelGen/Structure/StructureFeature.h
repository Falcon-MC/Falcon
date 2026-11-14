#pragma once

#include "World/Level/Dimension/Dimension.h"

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

class ChunkGenerator;

struct StructureChunkPos {
    int32_t x = 0;
    int32_t z = 0;
};

// one candidate chunk per square region, from the world seed, kept when it passes the structure's checks; reads
// only the seed and biome source: safe off the main thread
class StructureFeature {
public:
    enum class Spread {
        Linear,
        // the mean of two draws: pulls the start towards the middle of the region
        Triangular
    };

    enum class Generator {
        MersenneTwister,
        // a 48-bit linear congruential generator seeded with the whole region seed
        Congruential
    };

    StructureFeature(std::string name, int32_t spacing, int32_t separation, int32_t salt, Spread spread,
                     Generator generator = Generator::MersenneTwister);

    virtual ~StructureFeature() = default;

    const std::string &getName() const;

    // the dimension the structure generates in, the one /locate searches
    virtual DimensionType getDimension() const {
        return DimensionType::Overworld;
    }

    // the side of the square regions, in chunks, each holding one candidate start
    int32_t getSpacing() const {
        return mSpacing;
    }

    StructureChunkPos getCandidate(int64_t seed, int32_t regionX, int32_t regionZ) const;

    bool isStartChunk(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const;

    // candidates inside the chunk box, x then z ascending, before the structure's checks: one draw per region
    std::vector<StructureChunkPos> getCandidatesIn(int64_t seed, int32_t minChunkX, int32_t minChunkZ,
                                                   int32_t maxChunkX, int32_t maxChunkZ) const;

    // the first valid start of the region rings around the block position, which is what /locate structure reports
    bool findNearest(const ChunkGenerator &generator, int32_t blockX, int32_t blockZ, StructureChunkPos &out) const;

    // the random a chunk post processes one structure type with, seeded anew for every type
    static std::mt19937 createPostProcessRandom(int64_t seed, int32_t chunkX, int32_t chunkZ);

    // the pieces seed their own randoms with it
    static uint32_t getPostProcessSeed(int64_t seed, int32_t chunkX, int32_t chunkZ);

    // the region random as the candidate draws left it, which the start of the structure goes on drawing from
    std::mt19937 createStartRandom(int64_t seed, int32_t chunkX, int32_t chunkZ) const;

    static const std::vector<const StructureFeature *> &getAll();

    static const StructureFeature *get(const std::string &name);

protected:
    virtual bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const = 0;

    // true when the biomes sampled every four blocks in the cube of the given radius around the block pass the filter
    static bool _hasOnlyBiomes(const ChunkGenerator &generator, int32_t blockX, int32_t blockY, int32_t blockZ,
                               int32_t radius, const std::function<bool(int32_t biomeId)> &filter);

private:
    StructureChunkPos _drawMersenneCandidate(int64_t seed, int32_t regionX, int32_t regionZ,
                                             std::mt19937 &random) const;

    static std::vector<const StructureFeature *> &_registry();

    std::string mName;
    int32_t mSpacing;
    int32_t mSeparation;
    int32_t mSalt;
    Spread mSpread;
    Generator mGenerator;
};
