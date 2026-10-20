#include "World/Level/LevelGen/Overworld/Feature/Ore/LegacyEmeraldOreFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &emeraldOreState() {
        static const BlockState state = VanillaBlocks::EMERALD_ORE().toBlockState();
        return state;
    }
}

int32_t LegacyEmeraldOreFeature::getBase() const {
    return -2000;
}

int32_t LegacyEmeraldOreFeature::getRandom() const {
    return 2015;
}

void LegacyEmeraldOreFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    const int32_t x = random.nextInt(15);
    const int32_t z = random.nextInt(15);
    const int32_t y = chunk.getHeight(x, z);
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return;

    if (chunk.getBlock(x, y, z).mName == "minecraft:stone")
        chunk.setBlock(x, y, z, emeraldOreState());
}

const char *LegacyEmeraldOreFeature::name() const {
    return "minecraft:extreme_hills_after_surface_emerald_ore_feature";
}
