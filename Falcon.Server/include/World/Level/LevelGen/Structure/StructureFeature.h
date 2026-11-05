#pragma once

#include <cstdint>
#include <string>
#include <vector>

class ChunkGenerator;

struct StructureChunkPos {
    int32_t x = 0;
    int32_t z = 0;
};

/**
 * A structure the world spreads over a grid of square regions. Each region holds one candidate chunk picked from
 * the world seed, and the structure starts there when the candidate passes the structure's own checks. Only reads
 * the generator's seed and biome source, so it is safe off the main thread.
 */
class StructureFeature {
public:
    enum class Spread {
        Linear,
        // the mean of two draws, which pulls the start towards the middle of the region
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

    StructureChunkPos getCandidate(int64_t seed, int32_t regionX, int32_t regionZ) const;

    // the start nearest to the block position within the search radius, as /locate structure reports it
    bool findNearest(const ChunkGenerator &generator, int32_t blockX, int32_t blockZ, StructureChunkPos &out) const;

    static const std::vector<const StructureFeature *> &getAll();

    static const StructureFeature *get(const std::string &name);

protected:
    virtual bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const = 0;

private:
    static std::vector<const StructureFeature *> &_registry();

    std::string mName;
    int32_t mSpacing;
    int32_t mSeparation;
    int32_t mSalt;
    Spread mSpread;
    Generator mGenerator;
};
