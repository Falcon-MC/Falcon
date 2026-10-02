#include "Block/Blocks/PointedDripstoneBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/BlockQuery.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

#include <string>

FALCON_REGISTER_BLOCK(PointedDripstoneBlock, 323);

using namespace BlockQuery;

namespace {
    const char *HANGING = "hanging";
    const char *THICKNESS = "dripstone_thickness";
    const int32_t DRIPSTONE_GROWTH_PER_MILLION = 11378;
    const int32_t MAX_TIP_SEARCH = 7;
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

bool PointedDripstoneBlock::isHanging(const BlockState &state) {
    return state.mStates.getBool(HANGING, false);
}

bool PointedDripstoneBlock::pointsTowards(const BlockState &state, bool hanging) {
    return matches(state.mName) && isHanging(state) == hanging;
}

bool PointedDripstoneBlock::findTip(Level &level, const Vector3i &root, bool hanging, Vector3i &tip) {
    const int32_t step = hanging ? -1 : 1;
    Vector3i current = root;

    for (int32_t index = 0; index < MAX_TIP_SEARCH; ++index) {
        const BlockState state = stateAt(level, current);
        if (!pointsTowards(state, hanging))
            return false;

        if (state.mStates.getString(THICKNESS, "tip") == "tip") {
            tip = current;
            return true;
        }

        current = offset(current, step);
    }

    return false;
}

bool PointedDripstoneBlock::canTipGrow(Level &level, const Vector3i &tip, bool hanging) {
    const Vector3i target = offset(tip, hanging ? -1 : 1);
    if (!isInRange(level, target))
        return false;

    const BlockState next = stateAt(level, target);
    if (LiquidView(next).isLiquid())
        return false;

    return DecorationSupport::isAir(next)
           || (pointsTowards(next, !hanging) && next.mStates.getString(THICKNESS, "tip") == "tip");
}

void PointedDripstoneBlock::grow(Level &level, const Vector3i &tip, bool hanging) {
    if (!canTipGrow(level, tip, hanging))
        return;

    const Vector3i target = offset(tip, hanging ? -1 : 1);
    const BlockState targetState = stateAt(level, target);

    if (!DecorationSupport::isAir(targetState)) {
        level.setBlock(tip, DecorationSupport::withState(stateAt(level, tip), THICKNESS, "merge"), false);
        level.setBlock(target, DecorationSupport::withState(targetState, THICKNESS, "merge"), false);
        refreshThickness(level, offset(tip, hanging ? 1 : -1), hanging);
        refreshThickness(level, offset(target, hanging ? -1 : 1), !hanging);
        return;
    }

    BlockState placed = VanillaBlocks::POINTED_DRIPSTONE().toBlockState();
    placed = DecorationSupport::withState(placed, HANGING, hanging ? 1 : 0);
    placed = DecorationSupport::withState(placed, THICKNESS, "tip");
    if (!BlockChangeSystem::change(level, target, placed, BlockChangeCause::Grow, true))
        return;
    refreshThickness(level, target, hanging);
    refreshThickness(level, offset(target, hanging ? -1 : 1), !hanging);
}

void PointedDripstoneBlock::growStalagmiteBelow(Level &level, const Vector3i &tip) {
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

void PointedDripstoneBlock::refreshThickness(Level &level, const Vector3i &position, bool hanging) {
    const int32_t step = hanging ? -1 : 1;
    Vector3i current = position;

    for (int32_t index = 0; index < 4; ++index) {
        const BlockState state = stateAt(level, current);
        if (!pointsTowards(state, hanging))
            return;

        const BlockState ahead = stateAt(level, offset(current, step));
        std::string thickness;

        if (pointsTowards(ahead, !hanging)) {
            thickness = "merge";
        } else if (!pointsTowards(ahead, hanging)) {
            thickness = "tip";
        } else {
            const std::string aheadThickness = ahead.mStates.getString(THICKNESS, "tip");
            if (aheadThickness == "tip" || aheadThickness == "merge")
                thickness = "frustum";
            else
                thickness = pointsTowards(stateAt(level, offset(current, -step)), hanging) ? "middle" : "base";
        }

        if (state.mStates.getString(THICKNESS, "tip") != thickness)
            level.setBlock(current, DecorationSupport::withState(state, THICKNESS, thickness.c_str()), false);

        current = offset(current, -step);
    }
}
