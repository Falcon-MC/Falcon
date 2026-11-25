#include "World/Level/Block/PointedDripstoneBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockRandomOffset.h"
#include "World/Level/Block/BlockQuery.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/WaterBlock.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

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
    const int32_t MAX_DRIP_DISTANCE = 10;
    const float WATER_DRIP_CHANCE = 0.42f;
    const float LAVA_DRIP_CHANCE = 0.07f;
    // the drop particle of each fluid, aimed at a cauldron
    const int32_t LEVEL_EVENT_DRIP = 2028;
    const int32_t DRIP_WATER = 0x1C;
    const int32_t DRIP_LAVA = 0x1D;
    // a drop lands this long after leaving the tip, on top of one tick per block
    const int64_t DRIP_FALL_TICKS = 50;

    Vector3i offset(const Vector3i &position, int32_t dy) {
        return Vector3i(position.x, position.y + dy, position.z);
    }
}

bool PointedDripstoneBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

void PointedDripstoneBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    if (!isHanging(state) || matches(stateAt(level, offset(position, 1)).mName))
        return;

    growFromRoot(level, position);

    CauldronBlock::Liquid fluid;
    if (!getDripFluid(level, position, fluid))
        return;
    if (RandomTickSystem::nextFloat() > (fluid == CauldronBlock::Liquid::Lava ? LAVA_DRIP_CHANCE : WATER_DRIP_CHANCE))
        return;
    dripIntoCauldron(owner, level, position, fluid);
}

void PointedDripstoneBlock::growFromRoot(Level &level, const Vector3i &root) const {
    if (RandomTickSystem::nextInt(1000000) >= DRIPSTONE_GROWTH_PER_MILLION)
        return;

    if (stateAt(level, offset(root, 1)).mName != "minecraft:dripstone_block"
        || !WaterBlock::isSource(stateAt(level, offset(root, 2))))
        return;

    Vector3i tip;
    if (!findTip(level, root, true, tip) || !canTipGrow(level, tip, true))
        return;

    if (RandomTickSystem::nextInt(2) == 0)
        grow(level, tip, true);
    else
        growStalagmiteBelow(level, tip);
}

void PointedDripstoneBlock::dripIntoCauldron(ServerNetworkHandler &owner, Level &level, const Vector3i &root,
                                             CauldronBlock::Liquid fluid) const {
    Vector3i tip;
    if (!findTip(level, root, true, tip, MAX_DRIP_DISTANCE))
        return;

    // the first block below the tip that is not air ends the fall
    Vector3i cauldron = tip;
    for (int32_t distance = 1;; ++distance) {
        cauldron.y = tip.y - distance;
        if (!isInRange(level, cauldron))
            return;
        const BlockState below = stateAt(level, cauldron);
        if (CauldronBlock::matches(below.mName) && CauldronBlock::canReceiveDrip(level, cauldron, fluid))
            break;
        if (!isAirAt(level, cauldron) || distance >= MAX_DRIP_DISTANCE)
            return;
    }

    owner.broadcastLevelEvent(level, LEVEL_EVENT_DRIP, Vector3f((float) tip.x, (float) tip.y, (float) tip.z),
                              fluid == CauldronBlock::Liquid::Lava ? DRIP_LAVA : DRIP_WATER);
    level.scheduleUpdate(cauldron, (int64_t) (tip.y - cauldron.y) + DRIP_FALL_TICKS);
}

bool PointedDripstoneBlock::isStalactiteTip(Level &level, const Vector3i &position) {
    const BlockState state = stateAt(level, position);
    if (!matches(state.mName) || !isHanging(state) || state.mStates.getString(THICKNESS, "tip") != "tip")
        return false;
    // a drop does not form under water
    BlockMaterialType type;
    const BlockState extra = level.getBlockStateAtLayer(position.x, position.y, position.z, 1);
    return !BlockMaterial::find(extra, type) || type != BlockMaterialType::Water;
}

bool PointedDripstoneBlock::getDripFluid(Level &level, const Vector3i &stalactite, CauldronBlock::Liquid &fluid) {
    const BlockState state = stateAt(level, stalactite);
    if (!matches(state.mName) || !isHanging(state))
        return false;

    // the block the column hangs from; the fluid sits on top of it
    Vector3i root = stalactite;
    for (int32_t index = 0;; ++index) {
        if (index >= MAX_DRIP_DISTANCE)
            return false;
        root = offset(root, 1);
        if (!isInRange(level, root))
            return false;
        const BlockState current = stateAt(level, root);
        if (!matches(current.mName))
            break;
        if (!isHanging(current))
            return false;
    }

    const Vector3i source = offset(root, 1);
    if (source.y > level.getMaxY())
        return false;
    const BlockState above = stateAt(level, source);
    BlockMaterialType type;
    if (!BlockMaterial::find(above, type) || (type != BlockMaterialType::Water && type != BlockMaterialType::Lava)
        || above.mStates.getInt("liquid_depth", 0) != 0)
        return false;
    fluid = type == BlockMaterialType::Lava ? CauldronBlock::Liquid::Lava : CauldronBlock::Liquid::Water;
    return true;
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
