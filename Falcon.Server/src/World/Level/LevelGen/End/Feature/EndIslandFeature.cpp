#include "World/Level/LevelGen/End/Feature/EndIslandFeature.h"

#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"
#include "World/Level/LevelGen/End/TheEndGenerator.h"
#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

EndIslandFeature::EndIslandFeature(int64_t seed, const TheEndSimplexNoise &islandNoise)
        : mSeed(seed), mIslandNoise(&islandNoise), mChunkRandom(seed), mIslandObject() {
}

const char *EndIslandFeature::name() const {
    return "the_end_island";
}

void EndIslandFeature::apply(ChunkGenerateContext &context) {
    if (mRoot == nullptr)
        return;

    LevelChunk &chunk = context.getChunk();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    if ((int64_t) chunkX * (int64_t) chunkX + (int64_t) chunkZ * (int64_t) chunkZ
        <= TheEndGeneratorConstants::MAIN_ISLAND_CHUNK_RADIUS_SQUARED)
        return;

    mChunkRandom.setSeed(mSeed ^ TheEndGenerator::chunkHash(chunkX, chunkZ));

    if (mChunkRandom.nextBoundedInt(14) != 0)
        return;

    if (TheEndGenerator::getIslandHeight(chunkX, chunkZ, 1, 1, *mIslandNoise) >= -20.0f)
        return;

    const int32_t baseX = chunkX << 4;
    const int32_t baseZ = chunkZ << 4;

    BlockManager object(context.getLevel());

    const int32_t firstX = baseX + 8 + mChunkRandom.nextBoundedInt(16);
    const int32_t firstY = 55 + mChunkRandom.nextBoundedInt(16);
    const int32_t firstZ = baseZ + 8 + mChunkRandom.nextBoundedInt(16);
    mIslandObject.generate(object, mChunkRandom, firstX, firstY, firstZ);

    if (mChunkRandom.nextBoundedInt(4) == 0) {
        const int32_t secondX = baseX + 8 + mChunkRandom.nextBoundedInt(16);
        const int32_t secondY = 55 + mChunkRandom.nextBoundedInt(16);
        const int32_t secondZ = baseZ + 8 + mChunkRandom.nextBoundedInt(16);
        mIslandObject.generate(object, mChunkRandom, secondX, secondY, secondZ);
    }

    queueObject(object);
}
