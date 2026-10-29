#include "World/Level/LevelGen/Overworld/Feature/Tree/GroveTreeFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Tree/SmallSpruceTree.h"
#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

const char *GroveTreeFeature::NAME = "minecraft:grove_spruce_tree_feature";

const char *GroveTreeFeature::name() const {
    return NAME;
}

ObjectGeneratorPtr GroveTreeFeature::getGenerator(IRandom &random) {
    (void) random;
    return std::make_shared<SmallSpruceTree>();
}

bool GroveTreeFeature::canSpawnHere(int32_t biomeId) const {
    return TreeBiomeTags::isGrove(biomeId);
}

int32_t GroveTreeFeature::getSplit() const {
    return 4;
}

void GroveTreeFeature::apply(ChunkGenerateContext &context) {
    GriddedFeature::apply(context);

    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    BlockManager object(level);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t y = chunk.getHeight(x, z);
            if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                continue;

            if (chunk.getBlock(x, y, z).mName == "minecraft:spruce_leaves")
                object.setBlockStateAt(x + (chunk.getX() << 4), y + 1, z + (chunk.getZ() << 4),
                                       VanillaBlocks::SNOW_LAYER().toBlockState());
        }
    }

    queueObject(object);
}
