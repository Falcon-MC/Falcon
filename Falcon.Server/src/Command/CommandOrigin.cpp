#include "Command/CommandOrigin.h"

#include "Actor/ServerPlayer.h"
#include "Server/Localization.h"

std::string CommandOrigin::getLocale() const {
    return Localization::DEFAULT_LOCALE;
}

void CommandOrigin::sendLocalized(const std::string &key, const std::vector<std::string> &parameters) {
    sendMessage(Localization::getInstance().translate(getLocale(), key, parameters));
}

Vector3f CommandOrigin::getPosition() {
    const ServerPlayer *player = asPlayer();
    return player == nullptr ? Vector3f(0.0f, 0.0f, 0.0f) : player->getPosition();
}

Vector3f CommandOrigin::getRotation() {
    const ServerPlayer *player = asPlayer();
    return player == nullptr ? Vector3f(0.0f, 0.0f, 0.0f) : player->getRotation();
}

Level *CommandOrigin::getLevel() {
    return nullptr;
}
