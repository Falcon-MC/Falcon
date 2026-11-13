#include "World/Level/LevelGen/Structure/BuriedTreasureStructure.h"

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"

#include <random>

namespace {
    const int32_t CENTRE = 8;
    // the search stops under this height
    const int32_t LOWEST_Y = 6;
    const char *LOOT_TABLE = "loot_tables/chests/buriedtreasure.json";

    bool isSolid(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
        return y >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y
               && StructureMaterials::isBuriedTreasureGround(chunk.getBlock(x, y, z));
    }

    // water and leaves looked through
    int32_t aboveTopSolid(const LevelChunk &chunk, int32_t x, int32_t z) {
        for (int32_t y = LevelChunk::MAX_Y; y >= LevelChunk::MIN_Y; y--) {
            if (StructureMaterials::stopsTopSolidSearch(chunk.getBlock(x, y, z), false, false))
                return y + 1;
        }
        return LevelChunk::MIN_Y;
    }
}

void BuriedTreasureStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *feature = StructureFeature::get("buried_treasure");
    if (feature == nullptr || !feature->isStartChunk(generator, chunk.getX(), chunk.getZ()))
        return;

    std::mt19937 random = StructureFeature::createPostProcessRandom(generator.getSeed(), chunk.getX(), chunk.getZ());
    int32_t y = aboveTopSolid(chunk, CENTRE, CENTRE) - FeatureRandom::nextInt(random, 2) - 4;

    // down to the first block with solid blocks over it and on its four sides
    int32_t chestY = y;
    while (true) {
        chestY = y;
        const bool buried = isSolid(chunk, CENTRE, y + 1, CENTRE) && isSolid(chunk, CENTRE, y, CENTRE - 1)
                            && isSolid(chunk, CENTRE + 1, y, CENTRE) && isSolid(chunk, CENTRE, y, CENTRE + 1)
                            && isSolid(chunk, CENTRE - 1, y, CENTRE);
        if (buried || y < LOWEST_Y)
            break;
        y--;
    }

    // then down onto a solid block
    for (int32_t below = chestY - 1; !isSolid(chunk, CENTRE, below, CENTRE) && below + 1 >= LOWEST_Y; below--)
        chestY = below;

    chunk.setBlock(CENTRE, chestY, CENTRE, VanillaBlocks::CHEST().toBlockState());
    const Vector3i position(chunk.getX() * 16 + CENTRE, chestY, chunk.getZ() * 16 + CENTRE);
    level.getBlockActors().getOrCreate<ChestBlockActor>(position).setLootTable(LOOT_TABLE,
                                                                               FeatureRandom::nextInt(random));
}
