#include "World/Level/LevelGen/Overworld/Feature/Decoration/DiscFeature.h"

#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>

double DiscFeature::getProbability() const {
    return 1.0;
}

int32_t DiscFeature::getBase() const {
    return 0;
}

int32_t DiscFeature::getRandom() const {
    return 0;
}

bool DiscFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y, int32_t z) const {
    (void) level;
    (void) x;
    (void) y;
    (void) z;

    return IFeature::isSupportDirt(support) || DecorationSupport::isSand(support);
}

int32_t DiscFeature::getY(LevelChunk &chunk, Level &level, int32_t x, int32_t z) const {
    (void) level;
    return chunk.getHeight(x, z);
}

void DiscFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    const int32_t randomX = random.nextInt(15);
    const int32_t randomZ = random.nextInt(15);
    const int32_t height = getY(chunk, level, randomX, randomZ);
    const int32_t sourceX = (chunkX << 4) + randomX;
    const int32_t sourceZ = (chunkZ << 4) + randomZ;
    const double probability = getProbability();
    const bool alwaysPlace = probability >= 1.0;

    if (!DecorationSupport::isAir(chunk.getBlock(randomX, height + 1, randomZ)))
        return;

    BlockManager object(level);
    const BlockState sourceBlock = getSourceBlock();
    const int32_t radius = randomRange(random, getMinRadius(), getMaxRadius());
    const int32_t radiusSquared = radius * radius;
    const int32_t minX = sourceX - radius;
    const int32_t maxX = sourceX + radius;
    const int32_t minZ = sourceZ - radius;
    const int32_t maxZ = sourceZ + radius;
    const int32_t minChunkX = minX >> 4;
    const int32_t maxChunkX = maxX >> 4;
    const int32_t minChunkZ = minZ >> 4;
    const int32_t maxChunkZ = maxZ >> 4;
    bool placedAny = false;

    for (int32_t currentChunkX = minChunkX; currentChunkX <= maxChunkX; currentChunkX++) {
        const int32_t chunkStartX = currentChunkX << 4;
        const int32_t chunkEndX = chunkStartX + 15;
        const int32_t startX = std::max(minX, chunkStartX);
        const int32_t endX = std::min(maxX, chunkEndX);

        for (int32_t currentChunkZ = minChunkZ; currentChunkZ <= maxChunkZ; currentChunkZ++) {
            const int32_t chunkStartZ = currentChunkZ << 4;
            const int32_t chunkEndZ = chunkStartZ + 15;
            const int32_t startZ = std::max(minZ, chunkStartZ);
            const int32_t endZ = std::min(maxZ, chunkEndZ);

            LevelChunk *targetChunk = level.peekChunkPtr(currentChunkX, currentChunkZ);
            if (targetChunk == nullptr)
                continue;

            for (int32_t x = startX; x <= endX; x++) {
                const int32_t dx = x - sourceX;
                const int32_t dx2 = dx * dx;
                const int32_t localX = x & 15;

                for (int32_t z = startZ; z <= endZ; z++) {
                    const int32_t dz = z - sourceZ;
                    if (dx2 + dz * dz > radiusSquared)
                        continue;

                    if (!alwaysPlace && random.nextDouble() >= probability)
                        continue;

                    const int32_t localZ = z & 15;
                    const int32_t supportY = getY(*targetChunk, level, localX, localZ);
                    if (!DecorationSupport::isAir(targetChunk->getBlock(localX, supportY + 1, localZ)))
                        continue;

                    const BlockState &supportBlock = targetChunk->getBlock(localX, supportY, localZ);
                    if (isSupportValid(supportBlock, level, x, supportY, z)) {
                        object.setBlockStateAt(x, supportY + 1, z, sourceBlock);
                        placedAny = true;
                    }
                }
            }
        }
    }

    if (placedAny)
        queueObject(object);
}
