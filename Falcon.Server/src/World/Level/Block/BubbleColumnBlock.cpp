#include "World/Level/Block/BubbleColumnBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/MagmaBlock.h"
#include "World/Level/Block/SoulSandBlock.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Block/VanillaBlocks.h"

FALCON_REGISTER_BLOCK(BubbleColumnBlock, 356);

namespace {
    template <typename T>
    bool is(const BlockState &state) {
        return dynamic_cast<const T *>(VanillaBlocks::fromState(state)) != nullptr;
    }

    // sub_1435B5C60: soul sand lifts, a column passes its own drag on, anything else pulls down
    bool dragOf(const BlockState &below) {
        if (is<SoulSandBlock>(below))
            return false;
        if (is<BubbleColumnBlock>(below))
            return below.mStates.getBool("drag_down", false);
        return true;
    }

    const BlockState &column(bool dragDown) {
        static const BlockState columns[2] = {BlockState(BubbleColumnBlock::IDENTIFIER,
                                                         [] {
                                                             Tag states = Tag::ofCompound();
                                                             states.putByte("drag_down", 0);
                                                             return states;
                                                         }()),
                                              BlockState(BubbleColumnBlock::IDENTIFIER, [] {
                                                  Tag states = Tag::ofCompound();
                                                  states.putByte("drag_down", 1);
                                                  return states;
                                              }())};
        return columns[dragDown ? 1 : 0];
    }

    const BlockState &waterSource() {
        static const BlockState water("minecraft:water", [] {
            Tag states = Tag::ofCompound();
            states.putInt("liquid_depth", 0);
            return states;
        }());
        return water;
    }
}

bool BubbleColumnBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool BubbleColumnBlock::growAbove(LiquidPhysicsSystem &ticks, const Vector3i &position) {
    const Vector3i above(position.x, position.y + 1, position.z);
    const BlockState &target = ticks.stateAt(above);
    const LiquidProperties water = LiquidProperties::of(target);
    if (is<BubbleColumnBlock>(target) || water.material != LiquidMaterial::Water || water.depth != 0)
        return false;

    const bool dragDown = dragOf(ticks.stateAt(position));
    // the water moves to layer 1 first, then the column takes layer 0
    ticks.write(above, 1, waterSource());
    ticks.write(above, 0, column(dragDown));
    return true;
}

// sub_1435B4130: a tick one later, and no column stands without a water source on layer 1
void BubbleColumnBlock::onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position,
                                    const BlockState &state) const {
    (void) state;
    ticks.queueTick(position, this, 1);
    const LiquidProperties water = LiquidProperties::of(ticks.stateAt(position, 1));
    if (water.material != LiquidMaterial::Water || water.depth != 0)
        ticks.write(position, 0, BlockState());
}

// sub_14A431360: one tick for a change below, two otherwise, never merged
void BubbleColumnBlock::onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position,
                                               const BlockState &state, const Vector3i &changed) const {
    (void) state;
    const bool below = changed.x == position.x && changed.z == position.z && changed.y < position.y;
    ticks.queueTick(position, this, below ? 1 : 2);
}

// sub_1435B46E0: gone without support, else one higher
void BubbleColumnBlock::onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position,
                                     const BlockState &state) const {
    if (!_isSupported(ticks, position, state)) {
        ticks.write(position, 0, BlockState());
        return;
    }
    growAbove(ticks, position);
}

// vfunc 976: magma under a pulling column, soul sand under a lifting one, or the same column
bool BubbleColumnBlock::_isSupported(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) {
    const BlockState &below = ticks.stateAt(Vector3i(position.x, position.y - 1, position.z));
    const bool dragDown = state.mStates.getBool("drag_down", false);
    if (is<MagmaBlock>(below))
        return dragDown;
    if (is<SoulSandBlock>(below))
        return !dragDown;
    return is<BubbleColumnBlock>(below) && below.mStates.getBool("drag_down", false) == dragDown;
}
