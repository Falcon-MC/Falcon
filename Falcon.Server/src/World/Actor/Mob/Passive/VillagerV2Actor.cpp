#include "World/Actor/Mob/Passive/VillagerV2Actor.h"

#include "World/Actor/ActorClassRegistry.h"
#include "Network/ServerNetworkHandler.h"

#include <string>

FALCON_REGISTER_ACTOR(VillagerV2Actor, VillagerV2Actor::IDENTIFIER);

namespace {
    const char *const TAG_WILLING = "Willing";
    const char *const BREAD = "minecraft:bread";
    const char *const CROPS[] = {"minecraft:carrot", "minecraft:potato", "minecraft:beetroot"};
    // one stack must hold it: the game never adds two slots up
    const int BREAD_NEEDED = 3;
    const int CROPS_NEEDED = 12;

    bool isBread(const ItemStack &item) {
        return item.mDefinition->getIdentifier() == BREAD;
    }

    bool isCrop(const ItemStack &item) {
        for (const char *crop: CROPS) {
            if (item.mDefinition->getIdentifier() == crop)
                return true;
        }
        return false;
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
        if ((isBread(item) && item.mCount >= BREAD_NEEDED) || (isCrop(item) && item.mCount >= CROPS_NEEDED))
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
    item.mCount -= isBread(item) ? BREAD_NEEDED : CROPS_NEEDED;
    equipment.setInventoryItem(slot, item.mCount > 0 ? item : ItemStack::air());
}
