#include "World/Level/LevelGen/End/Feature/EndGatewayFeature.h"

#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"
#include "World/Level/LevelGen/End/TheEndGenerator.h"
#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

EndGatewayFeature::EndGatewayFeature(int64_t seed, const TheEndSimplexNoise &islandNoise)
        : mSeed(seed), mIslandNoise(&islandNoise), mChunkRandom(seed), mGatewayObject() {
}

const char *EndGatewayFeature::name() const {
    return "the_end_gateway";
}

void EndGatewayFeature::apply(ChunkGenerateContext &context) {
    if (mRoot == nullptr)
        return;

    LevelChunk &chunk = context.getChunk();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    if ((int64_t) chunkX * (int64_t) chunkX + (int64_t) chunkZ * (int64_t) chunkZ
        <= TheEndGeneratorConstants::MAIN_ISLAND_CHUNK_RADIUS_SQUARED)
        return;

    mChunkRandom.setSeed(mSeed ^ TheEndGenerator::chunkHash(chunkX, chunkZ));

    if (TheEndGenerator::getIslandHeight(chunkX, chunkZ, 1, 1, *mIslandNoise) <= 40.0f)
        return;

    if (mChunkRandom.nextBoundedInt(700) != 0)
        return;

    const int32_t x = (chunkX << 4) + mChunkRandom.nextBoundedInt(16);
    const int32_t z = (chunkZ << 4) + mChunkRandom.nextBoundedInt(16);
    const int32_t y = mRoot->getHeightAt(x, z) + mChunkRandom.nextBoundedInt(7) + 3;

    if (y <= 1 || y >= 254)
        return;

    BlockManager object(context.getLevel());
    mGatewayObject.generate(object, mChunkRandom, x, y, z);
    queueObject(object);
}
