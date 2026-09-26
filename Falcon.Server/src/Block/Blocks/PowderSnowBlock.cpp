#include "Block/Blocks/PowderSnowBlock.h"

#include "Actor/Actor.h"
#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(PowderSnowBlock, 185);

namespace {
    const Vector3f STUCK_MULTIPLIER(0.9f, 1.5f, 0.9f);
}

bool PowderSnowBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:powder_snow";
}

void PowderSnowBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                    const BlockState &state) const {
    (void) owner;
    (void) position;
    (void) state;
    actor.makeStuckInBlock(STUCK_MULTIPLIER);
}
