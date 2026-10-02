#include "Block/Blocks/CobwebBlock.h"

#include "Actor/Actor.h"
#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CobwebBlock, 185);

namespace {
    const Vector3f STUCK_MULTIPLIER(0.25f, 0.05f, 0.25f);
    const Vector3f WEAVING_STUCK_MULTIPLIER(0.5f, 0.25f, 0.5f);
}

bool CobwebBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:web";
}

void CobwebBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                const BlockState &state) const {
    (void) owner;
    (void) position;
    (void) state;
    Vector3f multiplier;
    getStuckMultiplier(actor, multiplier);
    actor.makeStuckInBlock(multiplier);
    actor.resetFallDistance();
}

bool CobwebBlock::getStuckMultiplier(const Actor &actor, Vector3f &multiplier) const {
    multiplier = actor.hasEffect(MobEffectId::Weaving) ? WEAVING_STUCK_MULTIPLIER : STUCK_MULTIPLIER;
    return true;
}
