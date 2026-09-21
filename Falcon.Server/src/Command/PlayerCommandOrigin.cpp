#include "Command/PlayerCommandOrigin.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

PlayerCommandOrigin::PlayerCommandOrigin(ServerNetworkHandler &handler, ServerPlayer &player,
                                         const CommandOriginData &origin)
        : mHandler(handler), mPlayer(player), mOrigin(origin) {}

const std::string &PlayerCommandOrigin::getSenderName() const {
    return mPlayer.getName();
}

void PlayerCommandOrigin::sendMessage(const std::string &message) {
    mPlayer.sendMessage(message);
}

void PlayerCommandOrigin::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    mPlayer.sendTranslation(key, parameters);
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
