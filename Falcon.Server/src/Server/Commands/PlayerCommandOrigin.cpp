#include "Server/Commands/PlayerCommandOrigin.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginManager.h"

PlayerCommandOrigin::PlayerCommandOrigin(ServerNetworkHandler &handler, ServerPlayer &player,
                                         const CommandOriginData &origin)
        : mHandler(handler), mPlayer(player), mOrigin(origin) {}

std::string PlayerCommandOrigin::getSenderName() const {
    return mPlayer.getName();
}

void PlayerCommandOrigin::sendMessage(const std::string &message) {
    if (mCollecting) {
        _collect(message, {}, false);
        return;
    }
    mPlayer.sendMessage(message);
}

void PlayerCommandOrigin::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    if (mCollecting) {
        _collect(key, parameters, false);
        return;
    }
    mPlayer.sendTranslation(key, parameters);
}

void PlayerCommandOrigin::sendError(const std::string &message) {
    if (mCollecting) {
        _collect(message, {}, true);
        return;
    }
    mHandler.sendCommandOutput(mPlayer, mOrigin, message, false);
}

void PlayerCommandOrigin::_collect(std::string text, std::vector<std::string> parameters, bool error) {
    CollectedMessage collected;
    collected.mMessage.mMessageId = std::move(text);
    collected.mMessage.mParameters = std::move(parameters);
    collected.mError = error;
    mCollected.push_back(std::move(collected));
}

void PlayerCommandOrigin::sendCollectedOutput(bool success) {
    mCollecting = false;
    if (mCollected.empty())
        return;
    std::vector<CommandOutputMessage> messages;
    messages.reserve(mCollected.size());
    for (CollectedMessage &collected: mCollected) {
        // the per-message bool is "Successful?"
        collected.mMessage.mInternal = success && !collected.mError;
        messages.push_back(std::move(collected.mMessage));
    }
    mCollected.clear();
    mHandler.sendCommandOutput(mPlayer, mOrigin, std::move(messages), success);
}

std::string PlayerCommandOrigin::getLocale() const {
    return mPlayer.getLocale();
}

CommandPermission PlayerCommandOrigin::getCommandPermission() const {
    return mPlayer.getCommandPermission();
}

bool PlayerCommandOrigin::canExecuteCommand(const Command &command) const {
    const auto group = mHandler.getPluginManager().getPermissions().commandPermission(mPlayer, command);
    return group.value_or(CommandOrigin::canExecuteCommand(command));
}

Level *PlayerCommandOrigin::getLevel() {
    return &mHandler.getLevelFor(mPlayer);
}
