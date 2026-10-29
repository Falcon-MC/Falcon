#include "World/Actor/Misc/EndCrystalActor.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

FALCON_REGISTER_ACTOR(EndCrystalActor, EndCrystalActor::IDENTIFIER);

EndCrystalActor::EndCrystalActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier) {
}

void EndCrystalActor::tick(ServerNetworkHandler &owner) {
    (void) owner;
}

bool EndCrystalActor::onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    (void) amount;
    (void) source;

    if (mExpired)
        return true;

    mExpired = true;

    Level &level = owner.getLevelFor(*this);

    Explosion explosion(owner, level, getPosition(), EXPLOSION_SIZE, this, false);
    explosion.explode();
    return true;
}
