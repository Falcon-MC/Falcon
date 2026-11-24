#pragma once

#include "Falcon/Plugin/LoadedPlugin.h"

#include <falcon/falcon_api.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class EventBus;
class ServerPlayer;
class Command;

class PluginPermissions {
public:
    explicit PluginPermissions(EventBus &bus);

    ~PluginPermissions();

    bool registerPermission(const std::string &node, FalconPermissionDefault defaultValue);

    std::optional<bool> permissionOverride(const ServerPlayer &player, const std::string &node) const;

    bool hasPermission(const ServerPlayer &player, const std::string &node) const;

    void setPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node, bool value);

    void unsetPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node);

    void clearPlugin(LoadedPlugin &plugin);

    bool reloadGroups();
    bool updateGroups(const std::vector<std::string> &arguments);
    std::vector<std::string> groupNames() const;
    std::optional<bool> commandPermission(const ServerPlayer &player, const Command &command) const;

private:
    using Attachments = std::unordered_map<std::string, std::unordered_map<LoadedPlugin *, bool>>;

    struct Group {
        std::set<std::string> mParents;
        std::map<std::string, bool> mNodes;
    };

    struct Groups {
        std::map<std::string, Group> mGroups;
        std::map<std::string, std::set<std::string>> mPlayers;
        std::string mDefault;

        bool valid() const;
        bool save(const std::filesystem::path &path) const;
    };

    std::optional<bool> _groupPermission(const ServerPlayer &player, const std::string &node) const;

    EventBus &mBus;
    std::filesystem::path mGroupsPath;
    Groups mGroups;
    bool mGroupsLoaded = false;
    uint32_t mLeaveHook = 0;
    std::unordered_map<std::string, FalconPermissionDefault> mDefaults;
    std::unordered_map<std::string, Attachments> mPlayers;
};
