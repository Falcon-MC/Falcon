#include "World/Level/LevelGen/Overworld/Feature/Decoration/WaterVegetationFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

void WaterVegetationFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();

    const int32_t randomX = random.nextInt(15);
    const int32_t randomZ = random.nextInt(15);
    const int32_t height = chunk.getHeight(randomX, randomZ);

    BlockState topBlockState = chunk.getBlock(randomX, height, randomZ);
    if (!(topBlockState == DecorationSupport::waterState()))
        return;

    int32_t depth = 0;
    while (topBlockState == DecorationSupport::waterState()) {
        depth++;
        topBlockState = chunk.getBlock(randomX, height - depth, randomZ);
    }

    const int32_t y = (height - depth) + 1;
    if (y > 0 && canStay(randomX, y, randomZ, chunk, level))
        placeBlock(randomX, y, randomZ, chunk, level, random);
}
