#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacyFlowerFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const BlockState &dandelionState() {
        static const BlockState state = VanillaBlocks::DANDELION().toBlockState();
        return state;
    }

    const BlockState &poppyState() {
        static const BlockState state = VanillaBlocks::POPPY().toBlockState();
        return state;
    }

}

const char *LegacyFlowerFeature::name() const {
    return "minecraft:scatter_overworld_flower_feature";
}

int32_t LegacyFlowerFeature::getBase() const {
    return -7;
}

int32_t LegacyFlowerFeature::getRandom() const {
    return 8;
}

void LegacyFlowerFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    const int32_t randomX = random.nextInt(15);
    const int32_t randomZ = random.nextInt(15);
    const int32_t sourceX = (chunkX << 4) + randomX;
    const int32_t sourceZ = (chunkZ << 4) + randomZ;

    BlockManager object(level);
    const int32_t radius = randomRange(random, 2, 3);
    const int32_t radiusSquared = radius * radius;

    const BlockState state = random.nextBoolean() ? dandelionState() : poppyState();

    for (int32_t x = sourceX - radius; x <= sourceX + radius; x++) {
        for (int32_t z = sourceZ - radius; z <= sourceZ + radius; z++) {
            if (!DecorationSupport::isChunkGenerated(level, x >> 4, z >> 4))
                continue;

            const int32_t dx = x - sourceX;
            const int32_t dz = z - sourceZ;
            if (dx * dx + dz * dz > radiusSquared || random.nextFloat() >= 0.2f)
                continue;

            const int32_t y = level.getHeightAt(x, z);
            const BlockState *targetState = level.peekBlockPtr(x, y + 1, z);
            if (targetState == nullptr || !DecorationSupport::isAir(*targetState))
                continue;

            const BlockState *support = level.peekBlockPtr(x, y, z);
            if (support != nullptr && isSupportDirt(*support))
                object.setBlockStateAt(x, y + 1, z, state);
        }
    }

    queueObject(object);
}
