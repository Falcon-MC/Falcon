#include "Block/Blocks/PointedDripstoneBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockQuery.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

#include <string>

FALCON_REGISTER_BLOCK(PointedDripstoneBlock, 323);

using namespace BlockQuery;

namespace {
    const char *THICKNESS = "dripstone_thickness";
    const int32_t DRIPSTONE_GROWTH_PER_MILLION = 11378;
    const int32_t MAX_STALAGMITE_SEARCH = 10;

    Vector3i offset(const Vector3i &position, int32_t dy) {
        return Vector3i(position.x, position.y + dy, position.z);
    }
}

bool PointedDripstoneBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void PointedDripstoneBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    (void) owner;

    if (!isHanging(state) || matches(stateAt(level, offset(position, 1)).mName))
        return;

    if (RandomTickSystem::nextInt(1000000) >= DRIPSTONE_GROWTH_PER_MILLION)
        return;

    if (stateAt(level, offset(position, 1)).mName != "minecraft:dripstone_block"
        || !WaterBlock::isSource(stateAt(level, offset(position, 2))))
        return;

    Vector3i tip;
    if (!findTip(level, position, true, tip) || !canTipGrow(level, tip, true))
        return;

    if (RandomTickSystem::nextInt(2) == 0)
        grow(level, tip, true);
    else
        growStalagmiteBelow(level, tip);
}

void PointedDripstoneBlock::growStalagmiteBelow(Level &level, const Vector3i &tip) const {
    Vector3i current = tip;

    for (int32_t index = 0; index < MAX_STALAGMITE_SEARCH; ++index) {
        current = offset(current, -1);
        if (!isInRange(level, current))
            return;

        const BlockState state = stateAt(level, current);
        if (LiquidView(state).isLiquid())
            return;

        if (pointsTowards(state, false)) {
            if (state.mStates.getString(THICKNESS, "tip") == "tip" && canTipGrow(level, current, false))
                grow(level, current, false);
            return;
        }

        if (!DecorationSupport::isAir(state))
            return;

        const Vector3i floor = offset(current, -1);
        if (level.isSolidAt(floor.x, floor.y, floor.z) && !LiquidView(stateAt(level, floor)).isLiquid()) {
            grow(level, floor, false);
            return;
        }
    }
}
