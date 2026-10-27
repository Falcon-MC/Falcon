#include "World/Level/LevelGen/Overworld/Feature/River/DiscGenerateFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <climits>

namespace {
    const int32_t LEVEL_MAX_HEIGHT = 319;
}

const BlockState &DiscGenerateFeature::stillWaterState() {
    static const BlockState state = VanillaBlocks::WATER().toBlockState();
    return state;
}

const BlockState &DiscGenerateFeature::stoneState() {
    static const BlockState state = VanillaBlocks::STONE().toBlockState();
    return state;
}

const BlockState &DiscGenerateFeature::dirtState() {
    static const BlockState state = VanillaBlocks::DIRT().toBlockState();
    return state;
}

const BlockState &DiscGenerateFeature::grassState() {
    static const BlockState state = VanillaBlocks::GRASS_BLOCK().toBlockState();
    return state;
}

double DiscGenerateFeature::getProbability() const {
    return 1.0;
}

const std::vector<const BlockState *> &DiscGenerateFeature::getReplacementBlocks() const {
    static const std::vector<const BlockState *> replacements = {&stoneState(), &dirtState(), &grassState()};
    return replacements;
}

int32_t DiscGenerateFeature::getRandom() const {
    return 0;
}

void DiscGenerateFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    if (random.nextDouble() >= getProbability())
        return;

    const int32_t randomX = random.nextInt(15);
    const int32_t randomZ = random.nextInt(15);
    const int32_t height = chunk.getHeight(randomX, randomZ);
    if (height < LevelChunk::MIN_Y || height > LevelChunk::MAX_Y)
        return;

    const int32_t sourceX = (chunkX << 4) + randomX;
    const int32_t sourceZ = (chunkZ << 4) + randomZ;
    if (!(chunk.getBlock(randomX, height, randomZ) == stillWaterState()))
        return;

    int32_t depth = 0;
    while (true) {
        const int32_t probeY = height - (depth + 1);
        depth++;
        if (probeY < LevelChunk::MIN_Y)
            return;

        if (!(chunk.getBlock(randomX, probeY, randomZ) == stillWaterState()))
            break;
    }

    const int32_t sourceY = height - (++depth);
    const int32_t radiusY = getRadiusY();
    if (sourceY < radiusY)
        return;

    BlockManager object(level);
    const BlockState &sourceBlock = getSourceBlock();
    const std::vector<const BlockState *> &replacementBlocks = getReplacementBlocks();
    const int32_t radius = randomRange(random, getMinRadius(), getMaxRadius());
    const int32_t radiusSquared = radius * radius;
    const int32_t minY = sourceY - radiusY;
    const int32_t maxY = sourceY + radiusY;
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

                    const int32_t localZ = z & 15;
                    int32_t highestReplaced = INT_MIN;

                    for (int32_t y = minY; y <= maxY; y++) {
                        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                            continue;

                        const BlockState &currentState = targetChunk->getBlock(localX, y, localZ);
                        for (const BlockState *replaceBlockState: replacementBlocks) {
                            if (currentState == *replaceBlockState) {
                                object.setBlockStateAt(x, y, z, sourceBlock);
                                placedAny = true;
                                highestReplaced = y;
                                break;
                            }
                        }
                    }

                    if (highestReplaced != INT_MIN) {
                        for (int32_t y = highestReplaced + 1; y < LEVEL_MAX_HEIGHT; y++) {
                            const BlockState &aboveState = targetChunk->getBlock(localX, y, localZ);
                            if (!(aboveState == dirtState()) && !(aboveState == grassState()))
                                break;

                            object.setBlockStateAt(x, y, z, sourceBlock);
                        }
                    }
                }
            }
        }
    }

    if (placedAny)
        queueObject(object);
}
