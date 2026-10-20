#include "World/Level/LevelGen/Feature/FeatureBases.h"

#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const int32_t SEA_LEVEL = 63;

    int64_t chunkHash(int32_t x, int32_t z) {
        return (((int64_t) x) << 32) | ((int64_t) (uint32_t) z);
    }

    const BlockState &liveBlockAt(Level &level, int32_t x, int32_t y, int32_t z) {
        static const BlockState airState = VanillaBlocks::AIR().toBlockState();

        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
            return airState;

        LevelChunk *chunk = level.peekChunkPtr(x >> 4, z >> 4);
        if (chunk == nullptr)
            return airState;

        return chunk->getBlock(x & 15, y, z & 15);
    }

    bool isLiquidState(const BlockState &state) {
        return state.mName == "minecraft:water" || state.mName == "minecraft:flowing_water"
               || state.mName == "minecraft:lava" || state.mName == "minecraft:flowing_lava";
    }
}

void CountGenerateFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    mRandom.setSeed(level.getSeed() ^ chunkHash(chunk.getX(), chunk.getZ()) ^ javaStringHash(name()));

    const int32_t count = getBase() + mRandom.nextBoundedInt(getRandom());
    for (int32_t i = 0; i < count; i++)
        populate(context, mRandom);
}

int32_t ObjectGeneratorFeature::getMin() const {
    return 5;
}

int32_t ObjectGeneratorFeature::getMax() const {
    return 6;
}

bool ObjectGeneratorFeature::canSpawnHere(int32_t biomeId) const {
    (void) biomeId;
    return true;
}

bool ObjectGeneratorFeature::checkBlock(const BlockState &state, int32_t y) const {
    if (isLiquidState(state))
        return false;

    if (y <= SEA_LEVEL)
        return false;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return true;

    return !data->mSolid || data->mTransparent;
}

void ObjectGeneratorFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ (chunkHash(chunkX, chunkZ) + javaStringHash(name())));

    const int32_t amount = randomRange(mRandom, getMin(), getMax());
    BlockManager object(level);

    for (int32_t i = 0; i < amount; i++) {
        const int32_t localX = mRandom.nextInt(15);
        const int32_t localZ = mRandom.nextInt(15);
        int32_t y = chunk.getHeight(localX, localZ);
        if (y < LevelChunk::MIN_Y)
            continue;

        const int32_t worldX = localX + (chunkX << 4);
        const int32_t worldZ = localZ + (chunkZ << 4);

        if (!canSpawnHere((int32_t) chunk.getBiomeAt(localX, y, localZ)))
            continue;

        while (y > LevelChunk::MIN_Y && checkBlock(liveBlockAt(level, worldX, y, worldZ), y))
            y--;

        if (isSupportDirt(liveBlockAt(level, worldX, y, worldZ)))
            getGenerator(mRandom)->generate(object, mRandom, worldX, y + 1, worldZ);
    }

    queueObject(object);
}

int32_t GriddedFeature::getSplit() const {
    return 2;
}

int32_t GriddedFeature::splitLength() const {
    return 16 / getSplit();
}

int32_t GriddedFeature::getDistanceToNextField() const {
    return getSplit() > splitLength() ? splitLength() / 2 : getSplit();
}

void GriddedFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    BlockManager object(level);

    for (int32_t x = 0; x < getSplit(); x++) {
        for (int32_t z = 0; z < getSplit(); z++) {
            mRandom.setSeed(level.getSeed() ^ chunkHash(chunkX, chunkZ) ^ (x + z) ^ javaStringHash(name()));

            const int32_t placeX = getDistanceToNextField()
                                   + mRandom.nextInt(splitLength() - getDistanceToNextField())
                                   + (x * splitLength()) + (chunkX << 4);
            const int32_t placeZ = getDistanceToNextField()
                                   + mRandom.nextInt(splitLength() - getDistanceToNextField())
                                   + (z * splitLength()) + (chunkZ << 4);
            const int32_t placeY = object.getHeightAt(placeX, placeZ);
            if (placeY < LevelChunk::MIN_Y)
                continue;

            LevelChunk *target = level.peekChunkPtr(placeX >> 4, placeZ >> 4);
            if (target == nullptr)
                continue;

            if (!canSpawnHere((int32_t) target->getBiomeAt(placeX & 15, placeY, placeZ & 15)))
                continue;

            if (isSupportDirt(liveBlockAt(level, placeX, placeY, placeZ)))
                getGenerator(mRandom)->generate(object, mRandom, placeX, placeY + 1, placeZ);
        }
    }

    queueObject(object);
}

void MultiFeatureWrapper::apply(ChunkGenerateContext &context) {
    for (const char *featureName: getFeatures()) {
        const IFeaturePtr feature = GenerateFeatureRegistry::get(featureName);
        if (feature == nullptr)
            continue;

        feature->setRoot(mRoot);
        feature->apply(context);
    }
}

int32_t LegacyTreeGeneratorFeature::getMin() const {
    return 5;
}

int32_t LegacyTreeGeneratorFeature::getMax() const {
    return 6;
}

float LegacyTreeGeneratorFeature::getBeeNestChance() const {
    return 0.0f;
}

bool LegacyTreeGeneratorFeature::canSpawnHere(int32_t biomeId) const {
    (void) biomeId;
    return true;
}

void LegacyTreeGeneratorFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    const int32_t amount = randomRange(mRandom, getMin(), getMax());
    BlockManager manager(level);

    for (int32_t i = 0; i < amount; i++) {
        const int32_t localX = mRandom.nextInt(15);
        const int32_t localZ = mRandom.nextInt(15);
        const int32_t y = chunk.getHeight(localX, localZ);
        if (y < LevelChunk::MIN_Y)
            continue;

        BlockManager object(level);
        const int32_t worldX = localX + (chunkX << 4);
        const int32_t worldZ = localZ + (chunkZ << 4);

        if (!canSpawnHere((int32_t) chunk.getBiomeAt(localX, y, localZ)))
            continue;

        if (!isSupportDirt(liveBlockAt(level, worldX, y, worldZ)))
            continue;

        const TreeGeneratorPtr generator = getGenerator(mRandom);
        if (generator == nullptr)
            return;

        generator->generate(object, mRandom, worldX, y + 1, worldZ);
        manager.merge(object);
    }

    queueObject(manager);
}
