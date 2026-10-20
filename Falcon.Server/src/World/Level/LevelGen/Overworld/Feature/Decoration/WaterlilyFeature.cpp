#include "World/Level/LevelGen/Overworld/Feature/Decoration/WaterlilyFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const int32_t SEA_LEVEL = 63;

    const BlockState &waterlilyState() {
        static const BlockState state = VanillaBlocks::WATERLILY().toBlockState();
        return state;
    }

}

const char *WaterlilyFeature::name() const {
    return "minecraft:fixup_waterlily_position_feature";
}

int32_t WaterlilyFeature::getBase() const {
    return 4;
}

int32_t WaterlilyFeature::getRandom() const {
    return 2;
}

void WaterlilyFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();

    const int32_t x = random.nextInt(15);
    const int32_t z = random.nextInt(15);
    const int32_t y = chunk.getHeight(x, z);

    if (y != SEA_LEVEL)
        return;

    if (chunk.getBlock(x, y, z).mName != "minecraft:water")
        return;

    chunk.setBlock(x, y + 1, z, waterlilyState());
}
