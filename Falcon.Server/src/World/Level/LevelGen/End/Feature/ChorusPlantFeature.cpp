#include "World/Level/LevelGen/End/Feature/ChorusPlantFeature.h"

#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"
#include "World/Level/LevelGen/End/TheEndGenerator.h"
#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

ChorusPlantFeature::ChorusPlantFeature(int64_t seed, const TheEndSimplexNoise &islandNoise)
        : mSeed(seed), mIslandNoise(&islandNoise), mChorusTree() {
}

const char *ChorusPlantFeature::name() const {
    return "the_end_chorus_flower";
}

void ChorusPlantFeature::apply(ChunkGenerateContext &context) {
    if (mRoot == nullptr)
        return;

    LevelChunk &chunk = context.getChunk();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    if ((int64_t) chunkX * (int64_t) chunkX + (int64_t) chunkZ * (int64_t) chunkZ
        <= TheEndGeneratorConstants::MAIN_ISLAND_CHUNK_RADIUS_SQUARED)
        return;

    mRandom.setSeed(mSeed ^ TheEndGenerator::chunkHash(chunkX, chunkZ));

    if (TheEndGenerator::getIslandHeight(chunkX, chunkZ, 1, 1, *mIslandNoise) <= 40.0f)
        return;

    for (int32_t i = 0; i < mRandom.nextBoundedInt(5); i++) {
        const int32_t x = (chunkX << 4) + mRandom.nextBoundedInt(16);
        const int32_t z = (chunkZ << 4) + mRandom.nextBoundedInt(16);
        const int32_t y = mRoot->getHeightAt(x, z);
        if (y <= 0)
            continue;

        if (mRoot->getBlockAt(x, y + 1, z).mName != "minecraft:air")
            continue;

        if (mRoot->getBlockAt(x, y, z).mName != "minecraft:end_stone")
            continue;

        BlockManager object(context.getLevel());
        mChorusTree.generate(object, mRandom, x, y + 1, z, 8);
        queueObject(object);
    }
}
