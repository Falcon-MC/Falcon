#include "Block/Blocks/CobwebBlock.h"

#include "Actor/Actor.h"
#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CobwebBlock, 185);

namespace {
    const Vector3f STUCK_MULTIPLIER(0.25f, 0.05f, 0.25f);
}

bool CobwebBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:web";
}

void CobwebBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                const BlockState &state) const {
    (void) owner;
    (void) position;
    (void) state;
    actor.makeStuckInBlock(STUCK_MULTIPLIER);
    actor.resetFallDistance();
}
