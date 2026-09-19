#include "Level/Generator/Overworld/Feature/Decoration/SeagrassFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace {

    const BlockState &seagrassState() {
        static const BlockState state = VanillaBlocks::SEAGRASS().toBlockState();
        return state;
    }

    const BlockState &tallSeagrassTopState() {
        static const BlockState state = DecorationSupport::withState(seagrassState(), "sea_grass_type", "double_top");
        return state;
    }

    const BlockState &tallSeagrassBottomState() {
        static const BlockState state = DecorationSupport::withState(seagrassState(), "sea_grass_type", "double_bot");
        return state;
    }

}

const char *SeagrassFeature::name() const {
    return "minecraft:river_after_surface_seagrass_feature_rules";
}

int32_t SeagrassFeature::getBase() const {
    return 24;
}

int32_t SeagrassFeature::getRandom() const {
    return 0;
}

float SeagrassFeature::getTallSeagrassProbability() const {
    return 0.3f;
}

bool SeagrassFeature::canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level) const {
    (void) level;
    return canStay(x, y, z, chunk, false);
}

bool SeagrassFeature::canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, bool tallSeagrass) const {
    if (tallSeagrass)
        return chunk.getBlock(x, y, z) == DecorationSupport::waterState();

    return chunk.getBlock(x, y, z) == DecorationSupport::waterState()
           && DecorationSupport::isSolid(chunk.getBlock(x, y - 1, z));
}

void SeagrassFeature::placeBlock(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level, IRandom &random) {
    (void) level;

    if (y <= 0 || !canStay(x, y, z, chunk, false))
        return;

    if (random.nextDouble() < (double) getTallSeagrassProbability()) {
        if (canStay(x, y + 1, z, chunk, true)) {
            DecorationSupport::setBlockKeepingWater(chunk, x, y, z, tallSeagrassBottomState());
            DecorationSupport::setBlockKeepingWater(chunk, x, y + 1, z, tallSeagrassTopState());
        }

        return;
    }

    DecorationSupport::setBlockKeepingWater(chunk, x, y, z, seagrassState());
}
