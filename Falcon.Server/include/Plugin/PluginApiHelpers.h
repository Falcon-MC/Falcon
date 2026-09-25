#pragma once

#include "Plugin/PluginEvent.h"
#include "Plugin/PluginManager.h"

#include <falcon/falcon_api.h>

#include <array>
#include <string>

class Actor;
class CommandOrigin;
class ItemStack;
class Level;
class ServerNetworkHandler;
class ServerPlayer;

namespace PluginApiHelpers {
    inline const char *hold(std::string value) {
        thread_local std::array<std::string, 32> buffers;
        thread_local size_t next = 0;
        std::string &slot = buffers[next];
        next = (next + 1) % buffers.size();
        slot = std::move(value);
        return slot.c_str();
    }

    inline ServerNetworkHandler &owner() {
        return PluginManager::getInstance().getOwner();
    }

    inline LoadedPlugin *plugin(FalconPlugin *handle) {
        return PluginManager::fromHandle(handle);
    }

    inline ServerPlayer *player(FalconPlayer *handle) {
        return reinterpret_cast<ServerPlayer *>(handle);
    }

    inline FalconPlayer *toHandle(ServerPlayer *value) {
        return reinterpret_cast<FalconPlayer *>(value);
    }

    inline Actor *entity(FalconEntity *handle) {
        return reinterpret_cast<Actor *>(handle);
    }

    inline FalconEntity *toHandle(Actor *value) {
        return reinterpret_cast<FalconEntity *>(value);
    }

    inline Level *level(FalconLevel *handle) {
        return reinterpret_cast<Level *>(handle);
    }

    inline FalconLevel *toHandle(Level *value) {
        return reinterpret_cast<FalconLevel *>(value);
    }

    inline ItemStack *item(FalconItem *handle) {
        return reinterpret_cast<ItemStack *>(handle);
    }

    inline FalconItem *toHandle(ItemStack *value) {
        return reinterpret_cast<FalconItem *>(value);
    }

    inline PluginEvent *event(FalconEvent *handle) {
        return reinterpret_cast<PluginEvent *>(handle);
    }

    inline CommandOrigin *sender(FalconCommandSender *handle) {
        return reinterpret_cast<CommandOrigin *>(handle);
    }
}
