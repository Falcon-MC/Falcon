#include "World/Level/Block/SoulSandBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BubbleColumnBlock.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"

FALCON_REGISTER_BLOCK(SoulSandBlock, 185);

bool SoulSandBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:soul_sand";
}

// place and queued tick both grow the column (sub_148F92C90)
void SoulSandBlock::onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
    (void) state;
    BubbleColumnBlock::growAbove(ticks, position);
}

// sub_148F93390: water changing just above, two ticks later on the second queue
void SoulSandBlock::onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position,
                                           const BlockState &state, const Vector3i &changed) const {
    (void) state;
    const bool above = changed.x == position.x && changed.z == position.z && changed.y == position.y + 1;
    if (above && ticks.propertiesAt(changed).material == LiquidMaterial::Water)
        ticks.queueRandomTick(position, this, 2);
}

void SoulSandBlock::onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position, const BlockState &state) const {
    (void) state;
    BubbleColumnBlock::growAbove(ticks, position);
}
