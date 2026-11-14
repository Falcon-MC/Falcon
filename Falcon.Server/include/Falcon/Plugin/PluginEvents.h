#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

class Actor;
class ItemStack;
class Level;
class ServerPlayer;

// plugin events fired from more than one place of the server
namespace PluginEvents {
    // FALCON_EVENT_PLAYER_TELEPORT or _ENTITY_TELEPORT; false when cancelled, to takes the plugin's destination
    bool teleport(Actor &actor, Vector3f &to, const char *cause);

    // FALCON_EVENT_PLAYER_BUCKET_FILL or _EMPTY, before the world changes; false when cancelled
    bool bucket(ServerPlayer &player, Level &level, const Vector3i &position, bool fill, const ItemStack &bucket);

    // one event per armor slot that differs; previous holds the four slots from before the change
    void armorChanged(ServerPlayer &player, const ItemStack (&previous)[4]);
}
