#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/AllowList.h"
#include "Server/BanList.h"
#include "Server/PropertiesSettings.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    // the limit of /setmaxplayers
    constexpr uint32_t MAX_PLAYERS = 1024;
    constexpr double TICKS_PER_SECOND = 20.0;

    std::string textOf(const char *value) {
        return value == nullptr ? std::string() : std::string(value);
    }

    bool hasText(const char *value) {
        return value != nullptr && *value != '\0';
    }

    const char *nameAt(const std::vector<std::string> &names, uint32_t index) {
        return index < names.size() ? hold(names[index]) : nullptr;
    }

    // after the handler returns: the plugin may be iterating the players when it bans
    void kickBanned(const NetworkIdentifier &id, const ServerPlayer &target, const std::string &reason) {
        const std::string message = reason.empty() ? target.localize("falcon.disconnect.banned")
                                                   : target.localize("falcon.disconnect.bannedReason", {reason});
        owner().postToMainThread([id, message] { owner()._disconnect(id, DisconnectReason::Kicked, message); });
    }

    int serverBan(const char *name, const char *reason, const char *source) {
        if (!hasText(name))
            return 0;

        const std::string why = textOf(reason);
        ServerPlayer *target = owner().getPlayerByExactName(name);
        owner().getBanList().add(name, why, textOf(source), target != nullptr ? target->getXuid() : std::string());
        if (target != nullptr)
            kickBanned(target->getNetworkIdentifier(), *target, why);
        return 1;
    }

    int serverPardon(const char *name) {
        return hasText(name) && owner().getBanList().remove(name) ? 1 : 0;
    }

    int serverIsBanned(const char *name) {
        return hasText(name) && owner().getBanList().find(name) != nullptr ? 1 : 0;
    }

    uint32_t serverBanCount() {
        return (uint32_t) owner().getBanList().getNames().size();
    }

    const char *serverBannedName(uint32_t index) {
        return nameAt(owner().getBanList().getNames(), index);
    }

    int serverBanIp(const char *address, const char *reason, const char *source) {
        if (!hasText(address))
            return 0;

        const std::string why = textOf(reason);
        owner().getIpBanList().add(address, why, textOf(source));
        for (auto &entry: owner().getPlayers()) {
            if (entry.first.getAddress() == address)
                kickBanned(entry.first, entry.second, why);
        }
        return 1;
    }

    int serverPardonIp(const char *address) {
        return hasText(address) && owner().getIpBanList().remove(address) ? 1 : 0;
    }

    int serverIsIpBanned(const char *address) {
        return hasText(address) && owner().getIpBanList().find(address) != nullptr ? 1 : 0;
    }

    uint32_t serverIpBanCount() {
        return (uint32_t) owner().getIpBanList().getNames().size();
    }

    const char *serverBannedIp(uint32_t index) {
        return nameAt(owner().getIpBanList().getNames(), index);
    }

    int serverAllowListAdd(const char *name) {
        return hasText(name) && owner().getAllowList().add(name) ? 1 : 0;
    }

    int serverAllowListRemove(const char *name) {
        if (!hasText(name) || !owner().getAllowList().remove(name))
            return 0;
        owner().postToMainThread([] { owner().kickNotAllowListedPlayers(); });
        return 1;
    }

    uint32_t serverAllowListCount() {
        return (uint32_t) owner().getAllowList().getNames().size();
    }

    const char *serverAllowListName(uint32_t index) {
        return nameAt(owner().getAllowList().getNames(), index);
    }

    int serverAllowListEnabled() {
        return owner().getProperties().getAllowList() ? 1 : 0;
    }

    void serverSetAllowListEnabled(int enabled) {
        owner().setAllowListEnabled(enabled != 0);
    }

    const char *serverMotd() {
        return hold(owner().getMotd());
    }

    void serverSetMotd(const char *motd) {
        if (motd != nullptr)
            owner().setMotd(motd, owner().getAnnouncement().mSubName);
    }

    uint32_t serverMaxPlayers() {
        return (uint32_t) owner().getMaxPlayers();
    }

    // never below the players already in, like /setmaxplayers
    int serverSetMaxPlayers(uint32_t maxPlayers) {
        if (maxPlayers == 0 || maxPlayers > MAX_PLAYERS)
            return 0;

        uint32_t online = 0;
        for (const auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned())
                ++online;
        }
        owner().setMaxPlayers((int) std::max(maxPlayers, online));
        return 1;
    }

    double serverMillisecondsPerTick() {
        return owner().getMillisecondsPerTick();
    }

    double serverTps() {
        const double milliseconds = owner().getMillisecondsPerTick();
        return milliseconds <= 0.0 ? TICKS_PER_SECOND : std::min(TICKS_PER_SECOND, 1000.0 / milliseconds);
    }

    void serverShutdown() {
        owner().requestStop();
    }

    const LoadedPlugin *pluginNamed(const char *name) {
        if (name == nullptr)
            return nullptr;
        for (const std::unique_ptr<LoadedPlugin> &loaded: PluginManager::getInstance().getPlugins()) {
            if (loaded->mDescription.mName == name)
                return loaded.get();
        }
        return nullptr;
    }

    uint32_t serverPluginCount() {
        return (uint32_t) PluginManager::getInstance().getPlugins().size();
    }

    const char *serverPluginName(uint32_t index) {
        const auto &plugins = PluginManager::getInstance().getPlugins();
        return index < plugins.size() ? hold(plugins[index]->mDescription.mName) : nullptr;
    }

    const char *serverPluginVersion(const char *name) {
        const LoadedPlugin *found = pluginNamed(name);
        return found == nullptr ? nullptr : hold(found->mDescription.mVersion);
    }

    int serverPluginEnabled(const char *name) {
        const LoadedPlugin *found = pluginNamed(name);
        return found != nullptr && found->mEnabled ? 1 : 0;
    }
}

void PluginServerApi::fillAdmin(FalconServerApi &api) {
    api.serverBan = &serverBan;
    api.serverPardon = &serverPardon;
    api.serverIsBanned = &serverIsBanned;
    api.serverBanCount = &serverBanCount;
    api.serverBannedName = &serverBannedName;
    api.serverBanIp = &serverBanIp;
    api.serverPardonIp = &serverPardonIp;
    api.serverIsIpBanned = &serverIsIpBanned;
    api.serverIpBanCount = &serverIpBanCount;
    api.serverBannedIp = &serverBannedIp;
    api.serverAllowListAdd = &serverAllowListAdd;
    api.serverAllowListRemove = &serverAllowListRemove;
    api.serverAllowListCount = &serverAllowListCount;
    api.serverAllowListName = &serverAllowListName;
    api.serverAllowListEnabled = &serverAllowListEnabled;
    api.serverSetAllowListEnabled = &serverSetAllowListEnabled;
    api.serverMotd = &serverMotd;
    api.serverSetMotd = &serverSetMotd;
    api.serverMaxPlayers = &serverMaxPlayers;
    api.serverSetMaxPlayers = &serverSetMaxPlayers;
    api.serverTps = &serverTps;
    api.serverMillisecondsPerTick = &serverMillisecondsPerTick;
    api.serverShutdown = &serverShutdown;
    api.serverPluginCount = &serverPluginCount;
    api.serverPluginName = &serverPluginName;
    api.serverPluginVersion = &serverPluginVersion;
    api.serverPluginEnabled = &serverPluginEnabled;
}
