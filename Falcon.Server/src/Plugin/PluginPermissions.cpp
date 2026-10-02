#include "Plugin/PluginPermissions.h"

#include "Actor/ServerPlayer.h"
#include "Core/Event/EventBus.h"

PluginPermissions::PluginPermissions(EventBus &bus) : mBus(bus) {
    mLeaveHook = mBus.after().mPlayerLeave.subscribe([this](PlayerLeaveAfterEvent &source) {
        mPlayers.erase(source.mPlayerName);
    });
}

PluginPermissions::~PluginPermissions() {
    mBus.after().mPlayerLeave.unsubscribe(mLeaveHook);
}

bool PluginPermissions::registerPermission(const std::string &node, FalconPermissionDefault defaultValue) {
    if (node.empty() || defaultValue > FALCON_PERMISSION_DEFAULT_OPERATOR)
        return false;
    return mDefaults.emplace(node, defaultValue).second;
}

bool PluginPermissions::hasPermission(const ServerPlayer &player, const std::string &node) const {
    const auto attachments = mPlayers.find(player.getName());
    if (attachments != mPlayers.end()) {
        const auto values = attachments->second.find(node);
        if (values != attachments->second.end() && !values->second.empty()) {
            for (const auto &entry: values->second) {
                if (!entry.second)
                    return false;
            }
            return true;
        }
    }

    const auto registered = mDefaults.find(node);
    if (registered == mDefaults.end() || registered->second == FALCON_PERMISSION_DEFAULT_OPERATOR)
        return player.isOp();
    return registered->second == FALCON_PERMISSION_DEFAULT_TRUE;
}

void PluginPermissions::setPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node,
                                      bool value) {
    if (node.empty())
        return;
    mPlayers[player.getName()][node][&plugin] = value;
}

void PluginPermissions::unsetPermission(LoadedPlugin &plugin, const ServerPlayer &player, const std::string &node) {
    const auto attachments = mPlayers.find(player.getName());
    if (attachments == mPlayers.end())
        return;

    const auto values = attachments->second.find(node);
    if (values == attachments->second.end())
        return;

    values->second.erase(&plugin);
    if (values->second.empty())
        attachments->second.erase(values);
    if (attachments->second.empty())
        mPlayers.erase(attachments);
}

void PluginPermissions::clearPlugin(LoadedPlugin &plugin) {
    for (auto player = mPlayers.begin(); player != mPlayers.end();) {
        for (auto node = player->second.begin(); node != player->second.end();) {
            node->second.erase(&plugin);
            if (node->second.empty())
                node = player->second.erase(node);
            else
                ++node;
        }

        if (player->second.empty())
            player = mPlayers.erase(player);
        else
            ++player;
    }
}
