#include "World/Level/LevelGen/Overworld/Feature/Tree/TaigaTreeFeature.h"

#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/FallenTree.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SmallSpruceTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    bool isFullBlock(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        if (data == nullptr)
            return false;

        return data->mSolid && !data->mTransparent;
    }

}

const char *TaigaTreeFeature::NAME = "minecraft:taiga_surface_trees_feature";

const char *TaigaTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr TaigaTreeFeature::getGenerator(IRandom &random) {
    if (random.nextInt(100) == 0)
        return std::make_shared<FallenTree>(TreeWoodType::SPRUCE);

    return std::make_shared<SmallSpruceTree>();
}

bool TaigaTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isTaiga(biomeId);
}

void TaigaTreeFeature::apply(ChunkGenerateContext &context) {
    GriddedFeature::apply(context);

    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    BlockManager object(level);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t y = chunk.getHeight(x, z);
            if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                continue;

            if (!isFullBlock(chunk.getBlock(x, y, z)))
                continue;

            const int32_t cx = x + (chunk.getX() << 4);
            const int32_t cz = z + (chunk.getZ() << 4);

            if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::COLD_TAIGA)
                continue;

            const BlockState *above = mRoot == nullptr ? nullptr : mRoot->getCachedBlock(cx, y + 1, cz);
            if (above == nullptr || above->mName == "minecraft:air")
                object.setBlockStateAt(cx, y + 1, cz, VanillaBlocks::SNOW_LAYER().toBlockState());
        }
    }

    queueObject(object);
}
