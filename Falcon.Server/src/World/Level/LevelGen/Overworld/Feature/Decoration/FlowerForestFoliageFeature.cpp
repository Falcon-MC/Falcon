#include "World/Level/LevelGen/Overworld/Feature/Decoration/FlowerForestFoliageFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    bool isLeaves(const BlockState &state) {
        return state.mName.find("leaves") != std::string::npos;
    }

    BlockState upperOf(const BlockState &base) {
        return DecorationSupport::withByteState(base, "upper_block_bit", true);
    }

}

const char *FlowerForestFoliageFeature::name() const {
    return "minecraft:flower_forest_first_foliage_feature";
}

int32_t FlowerForestFoliageFeature::getBase() const {
    return -1;
}

int32_t FlowerForestFoliageFeature::getRandom() const {
    return 8;
}

void FlowerForestFoliageFeature::_populateFlower(int32_t flower, BlockManager &object, int32_t x, int32_t y,
                                                 int32_t z, IRandom &random) {
    switch (flower) {
        case 1:
            object.setBlockStateAt(x, y, z, VanillaBlocks::ALLIUM().toBlockState());
            break;
        case 2:
            object.setBlockStateAt(x, y, z, VanillaBlocks::AZURE_BLUET().toBlockState());
            break;
        case 3:
            object.setBlockStateAt(x, y, z, VanillaBlocks::CORNFLOWER().toBlockState());
            break;
        case 4:
            object.setBlockStateAt(x, y, z, VanillaBlocks::DANDELION().toBlockState());
            break;
        case 5:
            object.setBlockStateAt(x, y, z, VanillaBlocks::LILY_OF_THE_VALLEY().toBlockState());
            break;
        case 6:
            object.setBlockStateAt(x, y, z, VanillaBlocks::OXEYE_DAISY().toBlockState());
            break;
        case 7:
            object.setBlockStateAt(x, y, z, VanillaBlocks::POPPY().toBlockState());
            break;
        case 8:
            object.setBlockStateAt(x, y, z, VanillaBlocks::ORANGE_TULIP().toBlockState());
            break;
        case 9:
            object.setBlockStateAt(x, y, z, VanillaBlocks::PINK_TULIP().toBlockState());
            break;
        case 10:
            object.setBlockStateAt(x, y, z, VanillaBlocks::RED_TULIP().toBlockState());
            break;
        case 11:
            object.setBlockStateAt(x, y, z, VanillaBlocks::WHITE_TULIP().toBlockState());
            break;
        case 12:
            if (random.nextBoolean()) {
                object.setBlockStateAt(x, y, z, VanillaBlocks::LILAC().toBlockState());
                object.setBlockStateAt(x, y + 1, z, upperOf(VanillaBlocks::LILAC().toBlockState()));
            }
            break;
        case 13:
            if (random.nextBoolean()) {
                object.setBlockStateAt(x, y, z, VanillaBlocks::PEONY().toBlockState());
                object.setBlockStateAt(x, y + 1, z, upperOf(VanillaBlocks::PEONY().toBlockState()));
            }
            break;
        case 14:
            if (random.nextBoolean()) {
                object.setBlockStateAt(x, y, z, VanillaBlocks::ROSE_BUSH().toBlockState());
                object.setBlockStateAt(x, y + 1, z, upperOf(VanillaBlocks::ROSE_BUSH().toBlockState()));
            }
            break;
        default:
            break;
    }
}

void FlowerForestFoliageFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    const int32_t randomX = random.nextInt(15);
    const int32_t randomZ = random.nextInt(15);
    const int32_t sourceX = (chunkX << 4) + randomX;
    const int32_t sourceZ = (chunkZ << 4) + randomZ;

    const int32_t flower = random.nextInt(14);

    BlockManager object(level);
    const int32_t radius = randomRange(random, 4, 5);

    for (int32_t x = sourceX - radius; x <= sourceX + radius; x++) {
        for (int32_t z = sourceZ - radius; z <= sourceZ + radius; z++) {
            if (!DecorationSupport::isChunkGenerated(level, x >> 4, z >> 4))
                return;

            if ((x - sourceX) * (x - sourceX) + (z - sourceZ) * (z - sourceZ) > radius * radius)
                continue;

            if (random.nextFloat() >= 0.3f)
                continue;

            int32_t depth = 0;
            const int32_t height = level.getHeightAt(x, z);
            const BlockState *topBlockState = level.peekBlockPtr(x, height, z);
            while (topBlockState != nullptr
                   && (isLeaves(*topBlockState) || DecorationSupport::isAir(*topBlockState))) {
                depth++;
                topBlockState = level.peekBlockPtr(x, height - depth, z);
            }

            if (topBlockState == nullptr || !isSupportGrass(*topBlockState))
                continue;

            _populateFlower(flower, object, x, (height - depth) + 1, z, random);
        }
    }

    queueObject(object);
}
