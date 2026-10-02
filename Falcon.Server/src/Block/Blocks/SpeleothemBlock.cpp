#include "Block/Blocks/SpeleothemBlock.h"

#include "Block/BlockQuery.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Systems/BlockChangeSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

#include <string>

using namespace BlockQuery;

namespace {
    const char *HANGING = "hanging";
    const char *THICKNESS = "dripstone_thickness";
    const int32_t MAX_TIP_SEARCH = 7;
    const int32_t THICKNESS_REFRESH_DEPTH = 4;

    Vector3i offset(const Vector3i &position, int32_t dy) {
        return Vector3i(position.x, position.y + dy, position.z);
    }
}

void SpeleothemBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    refreshThickness(level, position, isHanging(state));
    Block::onNeighbourChanged(owner, level, position, stateAt(level, position));
}

bool SpeleothemBlock::isHanging(const BlockState &state) {
    return state.mStates.getBool(HANGING, false);
}

bool SpeleothemBlock::pointsTowards(const BlockState &state, bool hanging) const {
    return state.mName == getIdentifier() && isHanging(state) == hanging;
}

bool SpeleothemBlock::findTip(Level &level, const Vector3i &root, bool hanging, Vector3i &tip) const {
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

bool SpeleothemBlock::canTipGrow(Level &level, const Vector3i &tip, bool hanging) const {
    const Vector3i target = offset(tip, hanging ? -1 : 1);
    if (!isInRange(level, target))
        return false;

    const BlockState next = stateAt(level, target);
    if (LiquidView(next).isLiquid())
        return false;

    return DecorationSupport::isAir(next)
           || (pointsTowards(next, !hanging) && next.mStates.getString(THICKNESS, "tip") == "tip");
}

void SpeleothemBlock::grow(Level &level, const Vector3i &tip, bool hanging) const {
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

    BlockState placed = toBlockState();
    placed = DecorationSupport::withState(placed, HANGING, hanging ? 1 : 0);
    placed = DecorationSupport::withState(placed, THICKNESS, "tip");
    if (!BlockChangeSystem::change(level, target, placed, BlockChangeCause::Grow, true))
        return;
    refreshThickness(level, target, hanging);
    refreshThickness(level, offset(target, hanging ? -1 : 1), !hanging);
}

void SpeleothemBlock::refreshThickness(Level &level, const Vector3i &position, bool hanging) const {
    const int32_t step = hanging ? -1 : 1;
    Vector3i current = position;

    for (int32_t index = 0; index < THICKNESS_REFRESH_DEPTH; ++index) {
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
