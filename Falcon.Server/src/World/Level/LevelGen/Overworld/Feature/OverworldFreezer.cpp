#include "World/Level/LevelGen/Overworld/Feature/OverworldFreezer.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/Biome/BiomeTemperature.h"

namespace {
    const BlockState &iceState() {
        static const BlockState state = VanillaBlocks::ICE().toBlockState();
        return state;
    }
}

void OverworldFreezer::apply(LevelChunk &chunk) {
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t y = _topRainBlock(chunk, x, z) - 1;
            if (_shouldFreeze(chunk, x, y, z))
                chunk.setBlock(x, y, z, iceState());
        }
    }
}

int32_t OverworldFreezer::_topRainBlock(const LevelChunk &chunk, int32_t x, int32_t z) {
    // LevelChunk::getTopRainBlockPos (sub_143868920): down from the highest non-empty section to the first block
    // whose precipitation interaction stops rain; only air is known here to let it through
    int32_t top = LevelChunk::MIN_Y;
    for (int32_t index = LevelChunk::SUB_CHUNK_COUNT - 1; index >= 0; index--) {
        if (!chunk.getSubChunk(index).isEmpty()) {
            top = LevelChunk::MIN_Y + (index << 4) + 15;
            break;
        }
    }

    for (int32_t y = top; y >= LevelChunk::MIN_Y; y--) {
        if (chunk.getBlock(x, y, z).mName != "minecraft:air")
            return y + 1;
    }
    return LevelChunk::MIN_Y;
}

bool OverworldFreezer::_shouldFreeze(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
    // BlockSource::shouldFreeze (sub_145E28EE0) without the neighbour test, as the post-processing calls it; its
    // block light limit (9) is not checked
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return false;

    const int32_t worldX = (chunk.getX() << 4) + x;
    const int32_t worldZ = (chunk.getZ() << 4) + z;
    if (BiomeTemperature::at((int32_t) chunk.getBiomeAt(x, y, z), worldX, y, worldZ) > 0.15f)
        return false;

    return WaterBlock::isSource(chunk.getBlock(x, y, z));
}
