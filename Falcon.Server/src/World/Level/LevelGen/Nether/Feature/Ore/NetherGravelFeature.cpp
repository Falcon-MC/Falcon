#include "World/Level/LevelGen/Nether/Feature/Ore/NetherGravelFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/Ore/OreFeature.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/LevelGen/Nether/NetherGeneratorConstants.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>

namespace {
    class NetherGravelOreShape : public OreFeature {
    public:
        NetherGravelOreShape(const BlockState &state, int32_t clusterSize)
                : mState(&state), mClusterSize(clusterSize) {
        }

        const BlockState &getState(const BlockState &original) const override {
            (void) original;

            return *mState;
        }

        int32_t getClusterCount() const override {
            return 0;
        }

        int32_t getClusterSize() const override {
            return mClusterSize;
        }

        int32_t getMinHeight() const override {
            return NetherGeneratorConstants::MIN_Y;
        }

        int32_t getMaxHeight() const override {
            return NetherGeneratorConstants::MAX_Y;
        }

        const char *name() const override {
            return "nether_gravel_shape";
        }

        void spawnAt(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
            spawn(manager, random, x, y, z);
        }

    private:
        const BlockState *mState;
        int32_t mClusterSize;
    };
}

const BlockState &NetherGravelFeature::getState() const {
    static const BlockState state = VanillaBlocks::GRAVEL().toBlockState();
    return state;
}

int32_t NetherGravelFeature::getClusterCount() const {
    return 2;
}

int32_t NetherGravelFeature::getClusterSize() const {
    return 33;
}

int32_t NetherGravelFeature::getMinHeight() const {
    return 5;
}

int32_t NetherGravelFeature::getMaxHeight() const {
    return 36;
}

const char *NetherGravelFeature::name() const {
    return "nether_gravel";
}

void NetherGravelFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(chunkHash(chunkX, chunkZ) ^ (level.getSeed() + javaStringHash(name())));

    const int32_t startX = chunkX << 4;
    const int32_t startZ = chunkZ << 4;
    const int32_t minY = std::max(getMinHeight(), NetherGeneratorConstants::MIN_Y);
    const int32_t maxY = std::min(getMaxHeight(), NetherGeneratorConstants::MAX_Y);

    NetherGravelOreShape shape(getState(), getClusterSize());
    BlockManager manager(level);

    for (int32_t i = 0; i < getClusterCount(); i++) {
        const int32_t x = startX + mRandom.nextInt(15);
        const int32_t z = startZ + mRandom.nextInt(15);
        const int32_t y = minY + mRandom.nextBoundedInt((maxY - minY) + 1);

        const BlockState *originalPtr = level.peekBlockPtr(x, y, z);
        const BlockState original = originalPtr == nullptr ? BlockState() : *originalPtr;
        if (!shape.canBeReplaced(original))
            continue;

        if (getBiomeIdAt(level, x, z) == NetherBiomeIds::BASALT_DELTAS)
            continue;

        mRandom.setSeed(level.getSeed() ^ chunkHash(chunkX, chunkZ) ^ (int64_t) (x + y + z));
        shape.spawnAt(manager, mRandom, x, y, z);
        mRandom.setSeed(level.getSeed() ^ chunkHash(chunkX, chunkZ));
    }

    queueObject(manager);
}
