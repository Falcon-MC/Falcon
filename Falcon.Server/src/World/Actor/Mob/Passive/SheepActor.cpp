#include "World/Actor/Mob/Passive/SheepActor.h"

#include "World/Actor/ActorClassRegistry.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"

FALCON_REGISTER_ACTOR(SheepActor, SheepActor::IDENTIFIER);

namespace {
    const char *const SHEARED_EVENT = "minecraft:on_sheared";
    const char *const SHEAR_LOOT_TABLE = "loot_tables/entities/sheep_shear.json";
}

void SheepActor::shearFromDispenser(ServerNetworkHandler &owner) {
    fireEvent(owner, SHEARED_EVENT);
    Level &level = owner.getLevelFor(*this);
    for (const ItemStack &stack: rollLoot(owner, SHEAR_LOOT_TABLE))
        owner.dropItem(level, getPosition(), stack, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
}
