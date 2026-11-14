#include "Falcon/Plugin/PluginEvents.h"

#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Level.h"

bool PluginEvents::teleport(Actor &actor, Vector3f &to, const char *cause) {
    ServerPlayer *player = dynamic_cast<ServerPlayer *>(&actor);
    const FalconEventType type = player != nullptr ? FALCON_EVENT_PLAYER_TELEPORT : FALCON_EVENT_ENTITY_TELEPORT;
    PluginManager *plugins = PluginManager::findWithSubscribers(type);
    if (plugins == nullptr)
        return true;

    PluginEvent event;
    event.mType = type;
    event.mCancellable = true;
    event.mPlayer = player;
    event.mEntity = &actor;
    event.mFrom = actor.getPosition();
    event.mTo = to;
    event.mCause = cause;
    plugins->dispatch(event);
    if (event.mCancelled)
        return false;
    if (event.mToChanged)
        to = event.mTo;
    return true;
}

bool PluginEvents::bucket(ServerPlayer &player, Level &level, const Vector3i &position, bool fill,
                          const ItemStack &bucket) {
    const FalconEventType type = fill ? FALCON_EVENT_PLAYER_BUCKET_FILL : FALCON_EVENT_PLAYER_BUCKET_EMPTY;
    PluginManager *plugins = PluginManager::findWithSubscribers(type);
    if (plugins == nullptr)
        return true;

    ItemStack copy = bucket;
    PluginEvent event;
    event.mType = type;
    event.mCancellable = true;
    event.mPlayer = &player;
    event.mLevel = &level;
    event.mItem = &copy;
    event.mBlockPosition = position;
    event.mBlockName = level.getBlockState(position.x, position.y, position.z).mName;
    plugins->dispatch(event);
    return !event.mCancelled;
}

void PluginEvents::armorChanged(ServerPlayer &player, const ItemStack (&previous)[4]) {
    PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_PLAYER_ARMOR_CHANGE);
    if (plugins == nullptr)
        return;

    for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot) {
        ItemStack before = previous[slot];
        ItemStack after = player.getInventory().getArmor(slot);
        if (before.isAir() && after.isAir())
            continue;
        if (!before.isAir() && !after.isAir() && before.mDefinition == after.mDefinition
            && before.mCount == after.mCount && before.mDamage == after.mDamage && before.mTag == after.mTag)
            continue;

        PluginEvent event;
        event.mType = FALCON_EVENT_PLAYER_ARMOR_CHANGE;
        event.mPlayer = &player;
        event.mDestinationSlot = slot;
        event.mItem = &after;
        event.mResult = &before;
        plugins->dispatch(event);
    }
}
