#include "World/Level/LevelGen/Overworld/Feature/Decoration/DoublePlantFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const BlockState &sunflowerLowerState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::SUNFLOWER().toBlockState(), "upper_block_bit", false);
        return state;
    }

    const BlockState &sunflowerUpperState() {
        static const BlockState state = DecorationSupport::withByteState(
                VanillaBlocks::SUNFLOWER().toBlockState(), "upper_block_bit", true);
        return state;
    }

}

const char *DoublePlantFeature::name() const {
    return "minecraft:sunflower_double_plant_patch_feature";
}

int32_t DoublePlantFeature::getBase() const {
    return -1;
}

int32_t DoublePlantFeature::getRandom() const {
    return 3;
}

void DoublePlantFeature::populate(ChunkGenerateContext &context, IRandom &random) {
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

    for (int32_t x = sourceX - radius; x <= sourceX + radius; x++) {
        for (int32_t z = sourceZ - radius; z <= sourceZ + radius; z++) {
            if ((x - sourceX) * (x - sourceX) + (z - sourceZ) * (z - sourceZ) > radius * radius)
                continue;

            if (random.nextFloat() >= 0.3f)
                continue;

            const int32_t height = level.getHeightAt(x, z);
            const BlockState *topBlockState = level.peekBlockPtr(x, height, z);
            if (topBlockState == nullptr || !isSupportGrass(*topBlockState))
                continue;

            object.setBlockStateAt(x, height + 1, z, sunflowerLowerState());
            object.setBlockStateAt(x, height + 2, z, sunflowerUpperState());
        }
    }

    queueObject(object);
}
