#include "Command/ServerCommandOrigin.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/Handler/ServerNetworkHandler.h"

ServerCommandOrigin::ServerCommandOrigin(ServerNetworkHandler *handler) : mHandler(handler) {}

Level *ServerCommandOrigin::getLevel() {
    return mHandler == nullptr ? nullptr : &mHandler->getLevel();
}

const std::string &ServerCommandOrigin::getSenderName() const {
    static const std::string name = "Console";
    return name;
}

void ServerCommandOrigin::sendMessage(const std::string &message) {
    LOG_INFO(LogAreaID::Server, "%s", message.c_str());
}

void ServerCommandOrigin::sendTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    std::string message = key;
    for (const std::string &parameter: parameters)
        message += " " + parameter;

    LOG_INFO(LogAreaID::Server, "%s", message.c_str());
}
