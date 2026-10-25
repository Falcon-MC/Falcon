#include "World/Item/FishingRodItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(FishingRodItem, 90);

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/FishingHookActor.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

FishingRodItem::FishingRodItem(const Item &base) : Item(base) {}

bool FishingRodItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

FishingHookActor *FishingRodItem::_findHook(ServerNetworkHandler &owner, const ServerPlayer &player) {
    for (auto &entry: owner.getActors()) {
        FishingHookActor *hook = dynamic_cast<FishingHookActor *>(entry.second.get());
        if (hook != nullptr && hook->isOwnedBy(player))
            return hook;
    }
    return nullptr;
}

bool FishingRodItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) item;

    FishingHookActor *hook = _findHook(owner, player);
    if (hook != nullptr) {
        const int32_t durability = hook->retrieve(owner, player);
        if (durability > 0)
            owner.damagePlayerHeldItem(player, durability);
        return true;
    }

    ServerActor *spawned = owner.spawnActor(owner.getLevelFor(player), FishingHookActor::IDENTIFIER,
                                            player.getPosition(), [&](ServerActor &actor) {
                FishingHookActor *created = dynamic_cast<FishingHookActor *>(&actor);
                if (created != nullptr)
                    created->cast(owner, player);
            });
    if (spawned == nullptr)
        return false;

    owner.playLevelSound(owner.getLevelFor(player), LevelSoundEvent::THROW, player.getPosition(),
                         "minecraft:player");
    return true;
}
