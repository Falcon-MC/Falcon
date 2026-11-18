#include "World/Level/Block/PointedDripstoneBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockRandomOffset.h"
#include "World/Level/Block/BlockQuery.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

#include <string>

FALCON_REGISTER_BLOCK(PointedDripstoneBlock, 323);

using namespace BlockQuery;

namespace {
    const char *THICKNESS = "dripstone_thickness";
    constexpr float TIP_INSET = 0.3125f;
    constexpr float FRUSTUM_INSET = 0.25f;
    constexpr float MIDDLE_INSET = 0.1875f;
    constexpr float BASE_INSET = 0.125f;
    constexpr float TIP_HEIGHT = 0.6875f;
    constexpr float OFFSET_RANGE = 0.125f;
    constexpr uint32_t OFFSET_STEPS = 16;
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

// the column narrows towards the tip and shifts with its position
bool PointedDripstoneBlock::getCollisionShapeAt(const BlockState &state, const Vector3i &position, const Actor *actor,
                                                AxisAlignedBB &shape) const {
    (void) actor;
    const std::string thickness = state.mStates.getString(THICKNESS, "tip");
    float inset = TIP_INSET;
    float bottom = 0.0f;
    float top = 1.0f;
    if (thickness == "frustum") {
        inset = FRUSTUM_INSET;
    } else if (thickness == "middle") {
        inset = MIDDLE_INSET;
    } else if (thickness == "base") {
        inset = BASE_INSET;
    } else if (thickness != "merge") {
        if (isHanging(state))
            bottom = 1.0f - TIP_HEIGHT;
        else
            top = TIP_HEIGHT;
    }

    const Vector3f offset = BlockRandomOffset::horizontal(position.x, position.z, OFFSET_RANGE, OFFSET_STEPS);
    shape = AxisAlignedBB(inset + offset.x, bottom, inset + offset.z, 1.0f - inset + offset.x, top,
                          1.0f - inset + offset.z);
    return true;
}
