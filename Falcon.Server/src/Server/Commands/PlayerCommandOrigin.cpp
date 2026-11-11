#include "Server/Commands/PlayerCommandOrigin.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

PlayerCommandOrigin::PlayerCommandOrigin(ServerNetworkHandler &handler, ServerPlayer &player,
                                         const CommandOriginData &origin)
        : mHandler(handler), mPlayer(player), mOrigin(origin) {}

std::string PlayerCommandOrigin::getSenderName() const {
    return mPlayer.getName();
}

void PlayerCommandOrigin::sendMessage(const std::string &message) {
    mPlayer.sendMessage(message);
}

void PlayerCommandOrigin::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    mPlayer.sendTranslation(key, parameters);
}

void PlayerCommandOrigin::sendError(const std::string &message) {
    mHandler.sendCommandOutput(mPlayer, mOrigin, message, false);
}

std::string PlayerCommandOrigin::getLocale() const {
    return mPlayer.getLocale();
}

CommandPermission PlayerCommandOrigin::getCommandPermission() const {
    return mPlayer.getCommandPermission();
}

Level *PlayerCommandOrigin::getLevel() {
    return &mHandler.getLevelFor(mPlayer);
}
