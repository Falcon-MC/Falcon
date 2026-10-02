#include "Command/ServerCommandOrigin.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/Localization.h"

#include <string>

namespace {
    const std::string FORMAT_PREFIX = "\xC2\xA7";
}

ServerCommandOrigin::ServerCommandOrigin(ServerNetworkHandler *handler) : mHandler(handler) {}

Level *ServerCommandOrigin::getLevel() {
    return mHandler == nullptr ? nullptr : &mHandler->getLevel();
}

std::string ServerCommandOrigin::getSenderName() const {
    return "Console";
}

void ServerCommandOrigin::sendMessage(const std::string &message) {
    LOG_INFO(LogAreaID::Server, "%s", message.c_str());
}

void ServerCommandOrigin::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    const std::string translated = Localization::getInstance().translate(key, parameters);

    std::string message;
    message.reserve(translated.size());
    for (size_t index = 0; index < translated.size(); ++index) {
        if (translated.compare(index, FORMAT_PREFIX.size(), FORMAT_PREFIX) == 0
            && index + FORMAT_PREFIX.size() < translated.size()) {
            index += FORMAT_PREFIX.size();
            continue;
        }
        message += translated[index];
    }

    LOG_INFO(LogAreaID::Server, "%s", message.c_str());
}
