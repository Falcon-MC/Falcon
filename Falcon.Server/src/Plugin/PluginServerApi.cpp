#include "Plugin/PluginServerApi.h"

#include "Actor/ServerPlayer.h"
#include "BuildInfo.h"
#include "Command/CommandOrigin.h"
#include "Core/Debug/BedrockLog.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginApiHelpers.h"

#include <string>

using namespace PluginApiHelpers;

namespace {
    ServerPlayer *spawnedPlayerAt(uint32_t index) {
        uint32_t current = 0;
        for (auto &entry: owner().getPlayers()) {
            if (!entry.second.isSpawned())
                continue;
            if (current == index)
                return &entry.second;
            current++;
        }
        return nullptr;
    }

    const char *serverVersion() {
        return FalconBuildInfo::kVersion;
    }

    void logMessage(FalconPlugin *source, FalconLogLevel level, const char *message) {
        const char *name = plugin(source)->mDescription.mName.c_str();
        const char *text = message == nullptr ? "" : message;
        if (level == FALCON_LOG_ERROR)
            LOG_ERROR(LogAreaID::Server, "[%s] %s", name, text);
        else if (level == FALCON_LOG_WARNING)
            LOG_WARN(LogAreaID::Server, "[%s] %s", name, text);
        else
            LOG_INFO(LogAreaID::Server, "[%s] %s", name, text);
    }

    const char *pluginName(FalconPlugin *source) {
        return plugin(source)->mDescription.mName.c_str();
    }

    const char *pluginDataFolder(FalconPlugin *source) {
        return plugin(source)->mDataFolder.c_str();
    }

    uint32_t onlinePlayerCount() {
        uint32_t count = 0;
        for (auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned())
                count++;
        }
        return count;
    }

    FalconPlayer *onlinePlayer(uint32_t index) {
        return toHandle(spawnedPlayerAt(index));
    }

    FalconPlayer *findPlayer(const char *name) {
        if (name == nullptr)
            return nullptr;
        return toHandle(owner().getPlayerByName(name));
    }

    const char *playerName(FalconPlayer *target) {
        return hold(player(target)->getName());
    }

    int playerIsOperator(FalconPlayer *target) {
        return player(target)->isOp() ? 1 : 0;
    }

    void playerSendMessage(FalconPlayer *target, const char *message) {
        if (message != nullptr)
            player(target)->sendMessage(message);
    }

    void playerKick(FalconPlayer *target, const char *reason) {
        const NetworkIdentifier id = player(target)->getNetworkIdentifier();
        const std::string text = reason == nullptr ? std::string() : std::string(reason);
        owner().postToMainThread([id, text] {
            owner()._disconnect(id, text);
        });
    }

    void broadcastMessage(const char *message) {
        if (message != nullptr)
            owner().broadcastSystemMessage(message);
    }

    uint64_t subscribe(FalconPlugin *source, FalconEventType type, FalconEventPriority priority, int ignoreCancelled,
                       FalconEventHandler handler, void *userData) {
        return PluginManager::getInstance().subscribe(*plugin(source), type, priority, ignoreCancelled != 0, handler,
                                                      userData);
    }

    void unsubscribe(uint64_t subscription) {
        PluginManager::getInstance().unsubscribe(subscription);
    }

    int registerCommand(FalconPlugin *source, const FalconCommandDescriptor *descriptor) {
        if (descriptor == nullptr)
            return 0;
        return PluginManager::getInstance().registerCommand(*plugin(source), *descriptor) ? 1 : 0;
    }

    const char *senderName(FalconCommandSender *source) {
        return hold(sender(source)->getSenderName());
    }

    FalconPlayer *senderPlayer(FalconCommandSender *source) {
        return toHandle(sender(source)->asPlayer());
    }

    void senderSendMessage(FalconCommandSender *source, const char *message) {
        if (message != nullptr)
            sender(source)->sendMessage(message);
    }

    uint64_t scheduleTask(FalconPlugin *source, FalconTask task, void *userData, uint64_t delayTicks,
                          uint64_t periodTicks) {
        if (task == nullptr)
            return 0;
        return PluginManager::getInstance().getScheduler().schedule(*plugin(source), task, userData, delayTicks,
                                                                    periodTicks);
    }

    uint64_t runAsync(FalconPlugin *source, FalconTask work, FalconTask done, void *userData) {
        if (work == nullptr)
            return 0;
        return PluginManager::getInstance().getScheduler().runAsync(*plugin(source), work, done, userData);
    }

    void cancelTask(uint64_t task) {
        PluginManager::getInstance().getScheduler().cancel(task);
    }

    FalconServerApi build() {
        FalconServerApi api{};
        api.size = (uint32_t) sizeof(FalconServerApi);
        api.versionMajor = FALCON_API_VERSION_MAJOR;
        api.versionMinor = FALCON_API_VERSION_MINOR;
        PluginServerApi::fillCore(api);
        PluginServerApi::fillEvents(api);
        PluginServerApi::fillEntities(api);
        PluginServerApi::fillPlayers(api);
        PluginServerApi::fillWorld(api);
        PluginServerApi::fillItems(api);
        PluginServerApi::fillPermissions(api);
        PluginServerApi::fillPackets(api);
        PluginServerApi::fillContent(api);
        return api;
    }
}

void PluginServerApi::fillCore(FalconServerApi &api) {
    api.serverVersion = &serverVersion;
    api.log = &logMessage;
    api.pluginName = &pluginName;
    api.pluginDataFolder = &pluginDataFolder;
    api.onlinePlayerCount = &onlinePlayerCount;
    api.onlinePlayer = &onlinePlayer;
    api.findPlayer = &findPlayer;
    api.playerName = &playerName;
    api.playerIsOperator = &playerIsOperator;
    api.playerSendMessage = &playerSendMessage;
    api.playerKick = &playerKick;
    api.broadcastMessage = &broadcastMessage;
    api.subscribe = &subscribe;
    api.unsubscribe = &unsubscribe;
    api.registerCommand = &registerCommand;
    api.senderName = &senderName;
    api.senderPlayer = &senderPlayer;
    api.senderSendMessage = &senderSendMessage;
    api.scheduleTask = &scheduleTask;
    api.runAsync = &runAsync;
    api.cancelTask = &cancelTask;
}

const FalconServerApi &PluginServerApi::get() {
    static const FalconServerApi api = build();
    return api;
}
