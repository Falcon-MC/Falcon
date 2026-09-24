#include "Block/Blocks/SnowLayerBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SnowLayerBlock, 340);

#include "Block/Blocks/PlacementHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"

using namespace PlacementHelpers;

namespace {
    const int SNOW_LAYER_MAX_HEIGHT = 7;
    const int SNOW_MELT_LIGHT_LEVEL = 12;
    const char *SNOW_LAYER_HEIGHT = "height";
}

bool SnowLayerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:snow_layer";
}

bool SnowLayerBlock::canBeReplaced(const BlockState &state) const {
    return state.mStates.getInt(SNOW_LAYER_HEIGHT, 0) < SNOW_LAYER_MAX_HEIGHT;
}

PlacementMergeResult SnowLayerBlock::mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                                    const Vector3f &clickPosition, Vector3i &position,
                                                    BlockState &state) const {
    (void) blockFace;
    (void) clickPosition;

    const Vector3i candidates[2] = {clickedPosition, position};
    for (const Vector3i &candidate: candidates) {
        const BlockState existing = stateAt(level, candidate);
        if (existing.mName != getIdentifier())
            continue;

        const int32_t height = existing.mStates.getInt(SNOW_LAYER_HEIGHT, 0);
        if (height >= SNOW_LAYER_MAX_HEIGHT)
            continue;

        Tag states = existing.mStates;
        states.putInt(SNOW_LAYER_HEIGHT, height + 1);
        position = candidate;
        state = BlockState(existing.mName, states);
        return PlacementMergeResult::Merged;
    }

    return PlacementMergeResult::None;
}

void SnowLayerBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    (void) owner;
    (void) state;

    if (RandomTickSystem::getBlockLight(level, position) >= SNOW_MELT_LIGHT_LEVEL)
        level.setBlock(position, BlockState("minecraft:air"), true);
}
