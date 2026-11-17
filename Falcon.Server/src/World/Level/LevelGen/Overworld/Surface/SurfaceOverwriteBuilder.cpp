#include "World/Level/LevelGen/Overworld/Surface/SurfaceOverwriteBuilder.h"

#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"

namespace {

    const BlockState &airState() {
        static const BlockState state = VanillaBlocks::AIR().toBlockState();
        return state;
    }

    const BlockState &snowLayerState() {
        static const BlockState state = VanillaBlocks::SNOW_LAYER().toBlockState();
        return state;
    }

}

void SurfaceOverwriteBuilder::apply(LevelChunk &chunk) const {
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t y = chunk.getHeight(x, z);
            if ((int32_t) chunk.getColumnBiome(x, z) != BiomeIds::ICE_PLAINS || !_isSolidState(chunk.getBlock(x, y, z)))
                continue;

            const BlockState above = chunk.getBlock(x, y + 1, z);
            if (above == airState())
                chunk.setBlock(x, y + 1, z, snowLayerState());
            else
                chunk.setBlock(x, y + 1, z, 1, snowLayerState());
        }
    }
}

bool SurfaceOverwriteBuilder::_isSolidState(const BlockState &state) {
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    return data != nullptr && data->mSolid;
}
