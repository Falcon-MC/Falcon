#include "World/Actor/Mob/Passive/VillagerV2Actor.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Item/Item.h"
#include "World/Item/VanillaItems.h"
#include "Network/ServerNetworkHandler.h"

#include <string>

FALCON_REGISTER_ACTOR(VillagerV2Actor, VillagerV2Actor::IDENTIFIER);

namespace {
    const char *const TAG_WILLING = "Willing";

    int32_t breedingCountOf(const ItemStack &item) {
        const Item *type = VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
        return type == nullptr ? 0 : type->getVillagerBreedingCount();
    }
}

bool VillagerV2Actor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!getFlags().get(ActorFlag::Sleeping))
        return MobActor::onInteract(owner, player);

    setWantsToWake(owner, true);
    return true;
}

Tag VillagerV2Actor::saveNbt() const {
    Tag data = PassiveActor::saveNbt();
    data.putByte(TAG_WILLING, mWilling ? 1 : 0);
    return data;
}

void VillagerV2Actor::loadNbt(const Tag &data) {
    PassiveActor::loadNbt(data);
    mWilling = data.getByte(TAG_WILLING, 0) != 0;
}

void VillagerV2Actor::setLovePartner(ServerNetworkHandler &owner, const Actor *partner) {
    mLovePartnerId = partner == nullptr ? NO_PARTNER : partner->getUniqueId();
    if (getFlags().get(ActorFlag::InLove) == (partner != nullptr))
        return;

    getFlags().set(ActorFlag::InLove, partner != nullptr);
    owner.syncActorFlags(*this);
}

int VillagerV2Actor::findBreedingFoodSlot() const {
    const MobEquipment &equipment = getEquipment();
    for (int slot = 0; slot < equipment.getInventorySize(); ++slot) {
        const ItemStack &item = equipment.getInventoryItem(slot);
        if (item.isAir())
            continue;
        const int32_t needed = breedingCountOf(item);
        if (needed > 0 && item.mCount >= needed)
            return slot;
    }
    return -1;
}

void VillagerV2Actor::consumeBreedingFood() {
    const int slot = findBreedingFoodSlot();
    if (slot < 0)
        return;

    MobEquipment &equipment = getEquipment();
    ItemStack item = equipment.getInventoryItem(slot);
    item.mCount -= breedingCountOf(item);
    equipment.setInventoryItem(slot, item.mCount > 0 ? item : ItemStack::air());
}
