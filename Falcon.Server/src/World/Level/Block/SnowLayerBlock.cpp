#include "World/Level/Block/SnowLayerBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(SnowLayerBlock, 340);

#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Misc/PaintingActor.h"
#include "World/Actor/Mob/Neutral/PolarBearActor.h"
#include "World/Actor/Mob/Passive/RabbitActor.h"
#include "World/Level/BaseGameVersion.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Level.h"

#include <algorithm>

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

// sub_14B27C5D0: one layer below its look, a single layer is walked through
bool SnowLayerBlock::getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const {
    const int height = std::min(SNOW_LAYER_MAX_HEIGHT, std::max(0, state.mStates.getInt(SNOW_LAYER_HEIGHT, 0)));

    // worlds before 1.20.20 keep the old steps: none, half, full
    int eighths = height;
    if (BaseGameVersion::current().isBefore(1, 20, 20))
        eighths = height == SNOW_LAYER_MAX_HEIGHT ? 8 : height >= 3 ? 4 : 0;

    const ServerActor *serverActor = dynamic_cast<const ServerActor *>(actor);
    const bool projectile = serverActor != nullptr && serverActor->isProjectile();
    if (dynamic_cast<const RabbitActor *>(actor) != nullptr || dynamic_cast<const PolarBearActor *>(actor) != nullptr)
        eighths = height;
    else if (dynamic_cast<const ItemActor *>(actor) != nullptr || projectile)
        eighths = height + 1;
    else if (dynamic_cast<const PaintingActor *>(actor) != nullptr)
        eighths = 0;

    shape = AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, (float) eighths / 8.0f, 1.0f);
    return true;
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
        BlockChangeSystem::change(level, position, BlockState("minecraft:air"), BlockChangeCause::Fade, true);
}
