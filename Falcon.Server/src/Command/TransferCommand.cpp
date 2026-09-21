#include "Command/TransferCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/TransferPacket.h"

#include <cstdlib>

namespace {
    const int DEFAULT_PORT = 19132;
}

TransferCommand::TransferCommand(ServerNetworkHandler &handler)
        : Command("transfer", "commands.transferserver.description", "/transfer <player> <address> [port]",
                  {"transferserver"}),
          mHandler(handler) {}

std::vector<CommandOverloadData> TransferCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("player"));
    overload.mParameters.push_back(makeTypedParameter("address", CommandParamType::String));
    overload.mParameters.push_back(makeTypedParameter("port", CommandParamType::Int, true));
    return {overload};
}

bool TransferCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    int port = DEFAULT_PORT;
    if (arguments.size() > 2) {
        char *end = nullptr;
        const long requested = std::strtol(arguments[2].c_str(), &end, 10);
        if (end == arguments[2].c_str() || *end != '\0' || requested < 0 || requested > 65535) {
            sender.sendTranslation("commands.transferserver.invalid.port", {});
            return false;
        }
        port = (int) requested;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    TransferPacket packet;
    packet.mAddress = arguments[1];
    packet.mPort = port;

    for (ServerPlayer *target: targets)
        mHandler.sendPacketTo(target->getNetworkIdentifier(), packet);

    sender.sendTranslation("commands.transferserver.successful", {});
    return true;
}
