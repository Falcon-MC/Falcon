#pragma once

#include "Plugin/LoadedPlugin.h"

#include <falcon/falcon_api.h>

#include <cstdint>
#include <string>
#include <unordered_map>

class EventBus;
class ServerPlayer;

class PluginPermissions {
public:
    explicit PluginPermissions(EventBus &bus);

    ~PluginPermissions();

    bool registerPermission(const std::string &node, FalconPermissionDefault defaultValue);

    bool hasPermission(const ServerPlayer &player, const std::string &node) const;

    void setPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node, bool value);

    void unsetPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node);

    void clearPlugin(LoadedPlugin &plugin);

private:
    using Attachments = std::unordered_map<std::string, std::unordered_map<LoadedPlugin *, bool>>;

    EventBus &mBus;
    uint32_t mLeaveHook = 0;
    std::unordered_map<std::string, FalconPermissionDefault> mDefaults;
    std::unordered_map<std::string, Attachments> mPlayers;
};
