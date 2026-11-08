#include "World/Level/Block/ConcretePowderBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"

FALCON_REGISTER_BLOCK(ConcretePowderBlock, 358);

bool ConcretePowderBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_concrete_powder");
}

void ConcretePowderBlock::onTickPlace(LiquidPhysicsSystem &ticks, const Vector3i &position,
                                      const BlockState &state) const {
    (void) state;
    _harden(ticks, position);
}

// hardens on a tick one later (sub_14316AAD0)
void ConcretePowderBlock::onTickNeighbourChanged(LiquidPhysicsSystem &ticks, const Vector3i &position,
                                                 const BlockState &state, const Vector3i &changed) const {
    (void) state;
    (void) changed;
    ticks.queueTick(position, this, 1);
}

void ConcretePowderBlock::onQueuedTick(LiquidPhysicsSystem &ticks, const Vector3i &position,
                                       const BlockState &state) const {
    (void) state;
    _harden(ticks, position);
}

// water on layer 0 above or beside, never below (sub_14771E480, sub_14771DA60)
void ConcretePowderBlock::_harden(LiquidPhysicsSystem &ticks, const Vector3i &position) const {
    static const int32_t sides[5][3] = {{0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
    bool touching = false;
    for (const auto &side: sides) {
        const Vector3i next(position.x + side[0], position.y + side[1], position.z + side[2]);
        if (ticks.propertiesAt(next).material == LiquidMaterial::Water) {
            touching = true;
            break;
        }
    }
    if (!touching)
        return;

    const std::string &powder = getIdentifier();
    const BlockState concrete(powder.substr(0, powder.size() - (sizeof("_powder") - 1)));
    if (ticks.allows(position, concrete, BlockChangeCause::Form))
        ticks.write(position, 0, concrete);
}
