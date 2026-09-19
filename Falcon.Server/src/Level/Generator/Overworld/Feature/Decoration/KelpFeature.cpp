#include "Level/Generator/Overworld/Feature/Decoration/KelpFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Generator/Feature/FeatureNoiseHolder.h"
#include "Level/Generator/Noise/SimplexNoise.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

#include <memory>

namespace {

    const BlockState &kelpState() {
        static const BlockState state = VanillaBlocks::KELP().toBlockState();
        return state;
    }

    const BlockState &kelpMaxAgeState() {
        static const BlockState state = DecorationSupport::withState(kelpState(), "kelp_age", 25);
        return state;
    }

    const SimplexNoise *kelpNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getKelp();
    }

}

const char *KelpFeature::name() const {
    return "minecraft:kelp_feature";
}

int32_t KelpFeature::getBase() const {
    return 30;
}

int32_t KelpFeature::getRandom() const {
    return 15;
}

bool KelpFeature::canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level) const {
    (void) level;
    return chunk.getBlock(x, y, z) == DecorationSupport::waterState();
}

void KelpFeature::placeBlock(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level, IRandom &random) {
    const SimplexNoise *noise = kelpNoise();
    if (noise == nullptr)
        return;

    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    if (noise->getValue((double) ((chunkX << 4) | x), 0.0, (double) ((chunkZ << 4) + z)) < 0.0f)
        return;

    if (!canStay(x, y + 1, z, chunk, level))
        return;

    if (!DecorationSupport::isSolid(chunk.getBlock(x, y - 1, z))
        && chunk.getBlock(x, y - 1, z).mName != "minecraft:kelp")
        return;

    const int32_t height = random.nextBoundedInt(10) + 1;
    for (int32_t h = 0; h <= height; h++) {
        if (!canStay(x, y + h, z, chunk, level))
            return;

        if (h == height || !(chunk.getBlock(x, y + h + 2, z) == DecorationSupport::waterState())) {
            const BlockState state = DecorationSupport::withState(kelpState(), "kelp_age",
                                                                  20 + random.nextBoundedInt(4));
            DecorationSupport::setBlockKeepingWater(chunk, x, y + h, z, state);
            return;
        }

        DecorationSupport::setBlockKeepingWater(chunk, x, y + h, z, kelpMaxAgeState());
    }
}
