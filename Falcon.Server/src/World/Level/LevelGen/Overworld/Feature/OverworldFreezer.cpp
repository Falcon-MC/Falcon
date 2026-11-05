#include "World/Level/LevelGen/Overworld/Feature/OverworldFreezer.h"

#include "World/Level/Block/BlockPrecipitation.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/IceBlock.h"
#include "World/Level/Block/LeavesBlock.h"
#include "World/Level/Block/SnowLayerBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Overworld/Biome/BiomeTemperature.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"

namespace {
    const int32_t FULL_SNOW_HEIGHT = 7;

    const BlockState &iceState() {
        static const BlockState state = VanillaBlocks::ICE().toBlockState();
        return state;
    }

    const BlockState &snowState() {
        static const BlockState state = VanillaBlocks::SNOW_LAYER().toBlockState();
        return state;
    }

    // TopSnowBlock::mayPlace (sub_14B27CB10) on the block below; the block property flags it also rejects
    // (0x10000050) have no counterpart here
    bool supportsSnow(const BlockState &below) {
        if (SnowLayerBlock::matches(below.mName))
            return below.mStates.getInt("height", 0) == FULL_SNOW_HEIGHT;

        if (VanillaBlocks::getAs<LeavesBlock>(below.mName) != nullptr)
            return true;

        // material ice; packed and blue ice are registered with the solid material
        if (IceBlock::matches(below.mName))
            return false;

        return BlockSupport::isSolid(below);
    }
}

void OverworldFreezer::apply(LevelChunk &chunk) {
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t y = _topRainBlock(chunk, x, z);
            if (_shouldFreeze(chunk, x, y - 1, z))
                chunk.setBlock(x, y - 1, z, iceState());
            if (!_shouldSnow(chunk, x, y, z))
                continue;

            // a snowlogged plant stays under the snow in the second layer; covered_bit stays unset because the
            // game's post-processing places the default snow layer (sub_14CDD4880)
            const BlockState covered = chunk.getBlock(x, y, z);
            if (!DecorationSupport::isAir(covered))
                chunk.setBlock(x, y, z, 1, covered);
            chunk.setBlock(x, y, z, snowState());
        }
    }
}

int32_t OverworldFreezer::_topRainBlock(const LevelChunk &chunk, int32_t x, int32_t z) {
    // LevelChunk::getTopRainBlockPos (sub_143868920): down from the highest non-empty section to the first block
    // that stops rain, in either layer
    int32_t top = LevelChunk::MIN_Y - 1;
    for (int32_t index = LevelChunk::SUB_CHUNK_COUNT - 1; index >= 0; index--) {
        if (!chunk.getSubChunk(index).isEmpty()) {
            top = LevelChunk::MIN_Y + (index << 4) + 15;
            break;
        }
    }

    for (int32_t y = top; y >= LevelChunk::MIN_Y; y--) {
        if (!BlockPrecipitation::letsRainThrough(chunk.getBlock(x, y, z)))
            return y + 1;

        const BlockState &extra = chunk.getBlock(x, y, z, 1);
        if (!DecorationSupport::isAir(extra) && !BlockPrecipitation::letsRainThrough(extra))
            return y + 1;
    }
    return LevelChunk::MIN_Y - 1;
}

bool OverworldFreezer::_isCold(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return false;

    const int32_t worldX = (chunk.getX() << 4) + x;
    const int32_t worldZ = (chunk.getZ() << 4) + z;
    // the game also wants a stored block light of at most 9, always true here: the chunk has no light yet while it
    // is post-processed
    return BiomeTemperature::at((int32_t) chunk.getBiomeAt(x, y, z), worldX, y, worldZ) <= 0.15f;
}

bool OverworldFreezer::_shouldFreeze(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
    // BlockSource::shouldFreeze (sub_145E28EE0) without the neighbour test, as the post-processing calls it
    return _isCold(chunk, x, y, z) && WaterBlock::isSource(chunk.getBlock(x, y, z));
}

bool OverworldFreezer::_shouldSnow(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
    // BlockSource::shouldSnow (sub_145E28D40)
    if (!_isCold(chunk, x, y, z) || y - 1 < LevelChunk::MIN_Y)
        return false;

    const BlockState &state = chunk.getBlock(x, y, z);
    if (!DecorationSupport::isAir(state) && !BlockPrecipitation::isSnowloggable(state))
        return false;

    return supportsSnow(chunk.getBlock(x, y - 1, z));
}
