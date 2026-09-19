#include "Block/BlockLightProperties.h"

#include "Block/BlockData.h"
#include "Block/BlockIdentifier.h"

namespace {
    const int32_t FILTER_SHIFT = 8;
    const int32_t DIFFUSES_BIT = 1 << 16;
    const int32_t TRANSPARENT_BIT = 1 << 17;
    const int32_t FULL_FILTER = 15;

    bool diffusesSkyLight(const std::string &identifier) {
        return identifier == "minecraft:web" || BlockIdentifier::endsWith(identifier, "_leaves")
               || identifier == "minecraft:azalea_leaves_flowered";
    }

    int lightFilterOverride(const std::string &identifier) {
        if (BlockIdentifier::equalsAny(identifier, {"minecraft:water", "minecraft:flowing_water", "minecraft:lava",
                                                    "minecraft:flowing_lava", "minecraft:ice",
                                                    "minecraft:frosted_ice"}))
            return 2;

        if (identifier == "minecraft:packed_ice")
            return FULL_FILTER;

        if (BlockIdentifier::equalsAny(identifier, {"minecraft:slime", "minecraft:honey_block",
                                                    "minecraft:barrier"}))
            return 1;

        if (identifier == "minecraft:cauldron")
            return 3;

        if (identifier == "minecraft:daylight_detector" || identifier == "minecraft:daylight_detector_inverted")
            return 0;

        return -1;
    }

    int32_t compute(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        if (data == nullptr)
            return TRANSPARENT_BIT;

        int filter = lightFilterOverride(state.mName);
        if (filter < 0)
            filter = data->mSolid && !data->mTransparent ? FULL_FILTER : 1;

        int32_t packed = (int32_t) (data->mLightEmission & 0xFF) | ((filter & 0xFF) << FILTER_SHIFT);
        if (diffusesSkyLight(state.mName))
            packed |= DIFFUSES_BIT;

        if (data->mTransparent)
            packed |= TRANSPARENT_BIT;

        return packed;
    }
}

int32_t BlockLightProperties::packed(const BlockState &state) {
    int32_t cached = state.getCachedLightProperties();
    if (cached < 0) {
        cached = compute(state);
        state.cacheLightProperties(cached);
    }

    return cached;
}
