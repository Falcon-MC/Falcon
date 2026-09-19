#include "Level/Generator/End/Feature/SpikeFeature.h"

#include "Core/Math/MathConstants.h"
#include "Level/Generator/End/Object/EndSpikeObject.h"
#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <cmath>

namespace {

    class LegacyJavaRandomSource {
    public:
        explicit LegacyJavaRandomSource(int64_t seed)
                : mSeed(0) {
            setSeed(seed);
        }

        void setSeed(int64_t seed) {
            mSeed = (seed ^ 0x5DEECE66DLL) & ((1LL << 48) - 1LL);
        }

        int32_t next(int32_t bits) {
            mSeed = (int64_t) (((uint64_t) mSeed * 0x5DEECE66DULL + 0xBULL) & (uint64_t) ((1LL << 48) - 1LL));
            return (int32_t) ((uint64_t) mSeed >> (48 - bits));
        }

        int64_t nextLong() {
            const int64_t high = (int64_t) next(32);
            const int64_t low = (int64_t) next(32);
            return (high << 32) + low;
        }

        int32_t nextInt(int32_t bound) {
            if ((bound & -bound) == bound)
                return (int32_t) (((int64_t) bound * (int64_t) next(31)) >> 31);

            int32_t bits = 0;
            int32_t value = 0;
            do {
                bits = next(31);
                value = bits % bound;
            } while ((int64_t) bits - (int64_t) value + (int64_t) (bound - 1) > 2147483647LL);

            return value;
        }

    private:
        int64_t mSeed;
    };

}

SpikeFeature::SpikeFeature(int64_t seed)
        : mSpikes(createSpikes(seed)) {
}

const char *SpikeFeature::name() const {
    return "the_end_obsidian_pillar";
}

std::vector<EndSpikeData> SpikeFeature::createSpikes(int64_t seed) {
    LegacyJavaRandomSource random(seed);
    random.setSeed(random.nextLong() & 65535LL);

    std::vector<int32_t> values;
    values.reserve(10);
    for (int32_t i = 0; i < 10; i++)
        values.push_back(i);

    for (int32_t i = (int32_t) values.size(); i > 1; i--) {
        const int32_t target = random.nextInt(i);
        const int32_t swap = values[(size_t) (i - 1)];
        values[(size_t) (i - 1)] = values[(size_t) target];
        values[(size_t) target] = swap;
    }

    std::vector<EndSpikeData> spikes;
    spikes.reserve(10);

    for (int32_t i = 0; i < 10; i++) {
        const int32_t value = values[(size_t) i];
        EndSpikeData spike;
        spike.mX = (int32_t) std::floor(
                42.0 * std::cos(2.0 * (-MathConstants::PI + MathConstants::PI / 10.0 * (double) i)));
        spike.mZ = (int32_t) std::floor(
                42.0 * std::sin(2.0 * (-MathConstants::PI + MathConstants::PI / 10.0 * (double) i)));
        spike.mRadius = 2 + value / 3;
        spike.mHeight = 76 + value * 3;
        spike.mGuarded = value == 1 || value == 2;
        spikes.push_back(spike);
    }

    return spikes;
}

void SpikeFeature::apply(ChunkGenerateContext &context) {
    if (mRoot == nullptr)
        return;

    LevelChunk &chunk = context.getChunk();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    for (const EndSpikeData &spike: mSpikes) {
        if ((spike.mX >> 4) != chunkX || (spike.mZ >> 4) != chunkZ)
            continue;

        BlockManager object(context.getLevel());
        EndSpikeObject pillar(spike.mRadius, spike.mHeight, spike.mGuarded);
        pillar.generate(object, mRandom, spike.mX, chunk.getHeight(spike.mX & 15, spike.mZ & 15), spike.mZ);
        queueObject(object);
    }
}
