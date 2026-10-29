#include "World/Level/LevelGen/Overworld/Feature/Decoration/PlainsFlowerFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

const char *PlainsFlowerFeature::name() const {
    return "minecraft:scatter_plains_flower_feature";
}

int32_t PlainsFlowerFeature::getBase() const {
    return -7;
}

int32_t PlainsFlowerFeature::getRandom() const {
    return 8;
}

void PlainsFlowerFeature::_populateFlower(int32_t flower, BlockManager &object, int32_t x, int32_t y, int32_t z) {
    switch (flower) {
        case 0:
            object.setBlockStateAt(x, y, z, VanillaBlocks::AZURE_BLUET().toBlockState());
            break;
        case 1:
            object.setBlockStateAt(x, y, z, VanillaBlocks::CORNFLOWER().toBlockState());
            break;
        case 2:
            object.setBlockStateAt(x, y, z, VanillaBlocks::DANDELION().toBlockState());
            break;
        case 3:
            object.setBlockStateAt(x, y, z, VanillaBlocks::OXEYE_DAISY().toBlockState());
            break;
        case 4:
            object.setBlockStateAt(x, y, z, VanillaBlocks::POPPY().toBlockState());
            break;
        case 5:
            object.setBlockStateAt(x, y, z, VanillaBlocks::ORANGE_TULIP().toBlockState());
            break;
        case 6:
            object.setBlockStateAt(x, y, z, VanillaBlocks::PINK_TULIP().toBlockState());
            break;
        case 7:
            object.setBlockStateAt(x, y, z, VanillaBlocks::RED_TULIP().toBlockState());
            break;
        case 8:
            object.setBlockStateAt(x, y, z, VanillaBlocks::WHITE_TULIP().toBlockState());
            break;
        default:
            break;
    }
}

void PlainsFlowerFeature::populate(ChunkGenerateContext &context, IRandom &random) {
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

            if (random.nextFloat() >= 0.1f)
                continue;

            const int32_t height = level.getHeightAt(x, z);
            const BlockState *topBlockState = level.peekBlockPtr(x, height, z);
            if (topBlockState == nullptr || !isSupportGrass(*topBlockState))
                continue;

            _populateFlower(random.nextBoundedInt(8), object, x, height + 1, z);
        }
    }

    queueObject(object);
}
