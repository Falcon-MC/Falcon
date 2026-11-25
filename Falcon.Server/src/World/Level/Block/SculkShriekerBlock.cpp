#include "World/Level/Block/SculkShriekerBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Actor/SculkShriekerBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(SculkShriekerBlock, 354);

bool SculkShriekerBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::SCULK_SHRIEKER;
}

const char *SculkShriekerBlock::getBlockActorId() const {
    return SculkShriekerBlockActor::BLOCK_ACTOR_ID;
}

// the listener lives in the block actor: a shrieker placed without one would stay deaf
void SculkShriekerBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) const {
    (void) owner;
    (void) state;
    level.getBlockActors().getOrCreate<SculkShriekerBlockActor>(position);
}

// anything but a projectile stepping on it shrieks for the player behind it
void SculkShriekerBlock::onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                  const BlockState &state) const {
    (void) state;
    const ServerActor *server = dynamic_cast<const ServerActor *>(&actor);
    if (server != nullptr && server->isProjectile())
        return;
    ServerPlayer *player = SculkShriekerBlockActor::responsiblePlayer(owner, &actor);
    if (player == nullptr)
        return;
    Level &level = owner.getLevelFor(actor);
    if (SculkShriekerBlockActor *shrieker = level.getBlockActors().find<SculkShriekerBlockActor>(position))
        shrieker->tryShriek(owner, level, *player);
}

// the shriek is over
void SculkShriekerBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                           const BlockState &state) const {
    if (state.mStates.getByte(STATE_ACTIVE, 0) == 0)
        return;
    Tag states = state.mStates;
    states.putByte(STATE_ACTIVE, 0);
    level.setBlock(position, BlockState(state.mName, states), true);
    if (SculkShriekerBlockActor *shrieker = level.getBlockActors().find<SculkShriekerBlockActor>(position))
        shrieker->respond(owner, level);
}
