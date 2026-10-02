#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSurfaceFeature.h"

#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Noise/SimplexNoise.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <algorithm>

namespace {
    const int32_t MAX_SURFACE_Y = 256;

    bool isExposed(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
        for (const DecorationSupport::FaceOffset &face: DecorationSupport::ALL_FACES) {
            const BlockState &neighbour = manager.getBlockAt(x + face.mX, y + face.mY, z + face.mZ);
            if (DecorationSupport::isAir(neighbour) || DecorationSupport::isWater(neighbour))
                return true;
        }
        return false;
    }
}

const char *IceCavesSurfaceFeature::name() const {
    return "minecraft:ice_caves_surface_feature";
}

void IceCavesSurfaceFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    const SimplexNoise *gradient = IceCavesSupport::gradientNoise();
    if (gradient == nullptr)
        return;

    BlockManager manager(context.getLevel());
    for (int32_t localX = 0; localX < 16; localX++) {
        const int32_t worldX = (chunk.getX() << 4) + localX;
        for (int32_t localZ = 0; localZ < 16; localZ++) {
            const int32_t worldZ = (chunk.getZ() << 4) + localZ;
            for (int32_t y = std::min(MAX_SURFACE_Y, chunk.getHeight(localX, localZ)); y > LevelChunk::MIN_Y + 1;
                 y--) {
                if (!IceCavesSupport::isIceCaves(chunk, worldX, y, worldZ))
                    continue;

                if (!IceCavesSupport::isNaturalBaseBlock(chunk.getBlock(localX, y, localZ)))
                    continue;

                if (!isExposed(manager, worldX, y, worldZ))
                    continue;

                const BlockState *state =
                        IceCavesSupport::gradientState(gradient->getValue((double) worldX, (double) y, (double) worldZ));
                if (state != nullptr)
                    manager.setBlockStateAt(worldX, y, worldZ, *state);
            }
        }
    }

    queueObject(manager);
}
