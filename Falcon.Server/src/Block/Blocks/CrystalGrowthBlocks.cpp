#include "Block/Blocks/CrystalGrowthBlocks.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

#include <string>

FALCON_REGISTER_BLOCK(BuddingAmethystBlock, 322);
FALCON_REGISTER_BLOCK(PointedDripstoneBlock, 323);

namespace {
    const char *BLOCK_FACE = "minecraft:block_face";
    const char *HANGING = "hanging";
    const char *THICKNESS = "dripstone_thickness";
    const int32_t BUD_GROWTH_CHANCE = 5;
    const int32_t DRIPSTONE_GROWTH_PER_MILLION = 11378;
    const int32_t MAX_TIP_SEARCH = 7;
    const int32_t MAX_STALAGMITE_SEARCH = 10;

    const char *BUD_TIERS[] = {
            "minecraft:small_amethyst_bud", "minecraft:medium_amethyst_bud", "minecraft:large_amethyst_bud",
            "minecraft:amethyst_cluster"
    };

    const Vector3i FACE_OFFSETS[] = {
            Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
            Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)
    };

    Vector3i offset(const Vector3i &position, int32_t dy) {
        return Vector3i(position.x, position.y + dy, position.z);
    }

    BlockState stateAt(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z);
    }

    bool isInRange(Level &level, const Vector3i &position) {
        return position.y >= level.getMinY() && position.y <= level.getMaxY();
    }

    bool isWaterSource(const BlockState &state) {
        return WaterBlock::matches(state.mName) && state.mStates.getInt("liquid_depth", 0) == 0;
    }

    BlockState budOfTier(int32_t tier, int32_t face) {
        BlockState bud;
        switch (tier) {
            case 0:
                bud = VanillaBlocks::SMALL_AMETHYST_BUD().toBlockState();
                break;
            case 1:
                bud = VanillaBlocks::MEDIUM_AMETHYST_BUD().toBlockState();
                break;
            case 2:
                bud = VanillaBlocks::LARGE_AMETHYST_BUD().toBlockState();
                break;
            default:
                bud = VanillaBlocks::AMETHYST_CLUSTER().toBlockState();
                break;
        }
        return DecorationSupport::withState(bud, BLOCK_FACE, PlacementOrientation::faceName(face));
    }

    int32_t budTier(const std::string &identifier) {
        for (int32_t tier = 0; tier < 4; ++tier) {
            if (identifier == BUD_TIERS[tier])
                return tier;
        }
        return -1;
    }
}

bool BuddingAmethystBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:budding_amethyst";
}

void BuddingAmethystBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                        const BlockState &state) const {
    (void) state;

    if (RandomTickSystem::nextInt(BUD_GROWTH_CHANCE) != 0)
        return;

    const int32_t face = RandomTickSystem::nextInt(6);
    const Vector3i &delta = FACE_OFFSETS[face];
    const Vector3i target(position.x + delta.x, position.y + delta.y, position.z + delta.z);
    if (!isInRange(level, target))
        return;

    const BlockState targetState = stateAt(level, target);

    if (DecorationSupport::isAir(targetState) || isWaterSource(targetState)) {
        level.setBlock(target, budOfTier(0, face), true);
        if (isWaterSource(targetState)) {
            level.setBlockStateAtLayer(target.x, target.y, target.z, 1, WaterBlock::source());
            BlockActionHandler::broadcastBlockUpdate(owner, level, target, WaterBlock::source(), 1);
        }
        return;
    }

    const int32_t tier = budTier(targetState.mName);
    if (tier < 0 || tier >= 3)
        return;

    if (targetState.mStates.getString(BLOCK_FACE, "") != PlacementOrientation::faceName(face))
        return;

    level.setBlock(target, budOfTier(tier + 1, face), true);
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
        || !isWaterSource(stateAt(level, offset(position, 2))))
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
    level.setBlock(target, placed, true);
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
