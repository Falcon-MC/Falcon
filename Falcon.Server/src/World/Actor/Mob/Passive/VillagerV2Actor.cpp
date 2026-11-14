#include "World/Actor/Mob/Passive/VillagerV2Actor.h"

#include "World/Actor/ActorClassRegistry.h"

FALCON_REGISTER_ACTOR(VillagerV2Actor, VillagerV2Actor::IDENTIFIER);

bool VillagerV2Actor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!getFlags().get(ActorFlag::Sleeping))
        return MobActor::onInteract(owner, player);

    setWantsToWake(owner, true);
    return true;
}
