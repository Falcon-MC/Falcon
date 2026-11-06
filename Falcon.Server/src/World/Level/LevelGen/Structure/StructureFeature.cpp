#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include "World/Level/LevelGen/ChunkGenerator.h"

#include <cstdlib>
#include <random>
#include <utility>

namespace {
    const int64_t REGION_X_MULTIPLIER = 341873128712LL;
    const int64_t REGION_Z_MULTIPLIER = 132897987541LL;
    const int32_t SEARCH_RINGS = 100;

    class CongruentialRandom {
    public:
        explicit CongruentialRandom(uint64_t seed) : mState((seed ^ MULTIPLIER) & MASK) {}

        int32_t nextInt(int32_t bound) {
            if ((bound & -bound) == bound)
                return (int32_t) (((int64_t) bound * (int64_t) _next()) >> 31);

            int32_t bits = 0;
            int32_t value = 0;
            do {
                bits = _next();
                value = bits % bound;
            } while ((int64_t) bits - value + (bound - 1) > INT32_MAX);

            return value;
        }

    private:
        static constexpr uint64_t MULTIPLIER = 0x5DEECE66DULL;
        static constexpr uint64_t MASK = (1ULL << 48) - 1ULL;

        int32_t _next() {
            mState = (mState * MULTIPLIER + 0xBULL) & MASK;
            return (int32_t) (mState >> 17);
        }

        uint64_t mState;
    };

    int32_t floorDiv(int32_t value, int32_t divisor) {
        int32_t quotient = value / divisor;
        if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
            --quotient;

        return quotient;
    }
}

StructureFeature::StructureFeature(std::string name, int32_t spacing, int32_t separation, int32_t salt, Spread spread,
                                   Generator generator)
        : mName(std::move(name)), mSpacing(spacing), mSeparation(separation), mSalt(salt), mSpread(spread),
          mGenerator(generator) {
    _registry().push_back(this);
}

const std::string &StructureFeature::getName() const {
    return mName;
}

StructureChunkPos StructureFeature::getCandidate(int64_t seed, int32_t regionX, int32_t regionZ) const {
    if (mGenerator == Generator::Congruential) {
        const uint64_t regionSeed = (uint64_t) regionX * (uint64_t) REGION_X_MULTIPLIER
                                    + (uint64_t) regionZ * (uint64_t) REGION_Z_MULTIPLIER + (uint64_t) seed
                                    + (uint64_t) mSalt;
        CongruentialRandom random(regionSeed);
        const int32_t bound = mSpacing - mSeparation;
        const int32_t offsetX = random.nextInt(bound);
        const int32_t offsetZ = random.nextInt(bound);
        return {regionX * mSpacing + offsetX, regionZ * mSpacing + offsetZ};
    }

    // the game seeds a plain 32-bit mersenne twister with the low half of the mixed region seed
    const uint64_t mixed = (uint64_t) regionX * (uint64_t) REGION_X_MULTIPLIER
                           + (uint64_t) regionZ * (uint64_t) REGION_Z_MULTIPLIER + (uint64_t) seed + (uint64_t) mSalt;
    std::mt19937 random((uint32_t) mixed);

    const uint32_t range = (uint32_t) (mSpacing - mSeparation);
    int32_t offsetX = 0;
    int32_t offsetZ = 0;
    if (mSpread == Spread::Triangular) {
        offsetX = (int32_t) ((random() % range + random() % range) / 2);
        offsetZ = (int32_t) ((random() % range + random() % range) / 2);
    } else {
        offsetX = (int32_t) (random() % range);
        offsetZ = (int32_t) (random() % range);
    }

    return {regionX * mSpacing + offsetX, regionZ * mSpacing + offsetZ};
}

bool StructureFeature::isStartChunk(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    const StructureChunkPos candidate = getCandidate(generator.getSeed(), floorDiv(chunkX, mSpacing),
                                                     floorDiv(chunkZ, mSpacing));
    return candidate.x == chunkX && candidate.z == chunkZ && _isValidStart(generator, chunkX, chunkZ);
}

bool StructureFeature::findNearest(const ChunkGenerator &generator, int32_t blockX, int32_t blockZ,
                                   StructureChunkPos &out) const {
    const int32_t originRegionX = floorDiv(blockX >> 4, mSpacing);
    const int32_t originRegionZ = floorDiv(blockZ >> 4, mSpacing);
    const int64_t seed = generator.getSeed();

    for (int32_t ring = 0; ring <= SEARCH_RINGS; ++ring) {
        for (int32_t dx = -ring; dx <= ring; ++dx) {
            for (int32_t dz = -ring; dz <= ring; ++dz) {
                if (std::abs(dx) != ring && std::abs(dz) != ring)
                    continue;

                const StructureChunkPos candidate = getCandidate(seed, originRegionX + dx, originRegionZ + dz);
                if (!_isValidStart(generator, candidate.x, candidate.z))
                    continue;

                out = candidate;
                return true;
            }
        }
    }

    return false;
}

const std::vector<const StructureFeature *> &StructureFeature::getAll() {
    return _registry();
}

const StructureFeature *StructureFeature::get(const std::string &name) {
    for (const StructureFeature *feature : _registry()) {
        if (feature->getName() == name)
            return feature;
    }

    return nullptr;
}

std::vector<const StructureFeature *> &StructureFeature::_registry() {
    static std::vector<const StructureFeature *> registry;
    return registry;
}
