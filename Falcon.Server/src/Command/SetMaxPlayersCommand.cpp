#include "Command/SetMaxPlayersCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cstdlib>

namespace {
    const int MAXIMUM_PLAYERS = 1024;
}

SetMaxPlayersCommand::SetMaxPlayersCommand(ServerNetworkHandler &handler)
        : Command("setmaxplayers", "commands.setmaxplayers.description", "/setmaxplayers <maxPlayers>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> SetMaxPlayersCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeTypedParameter("maxPlayers", CommandParamType::Int));
    return {overload};
}

bool SetMaxPlayersCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    char *end = nullptr;
    const long requested = std::strtol(arguments[0].c_str(), &end, 10);
    if (end == arguments[0].c_str() || *end != '\0') {
        sender.sendTranslation("commands.generic.num.invalid", {arguments[0]});
        return false;
    }

    int online = 0;
    for (const auto &entry: mHandler.getPlayers()) {
        if (entry.second.isSpawned())
            ++online;
    }

    int maxPlayers = (int) requested;
    if (maxPlayers > MAXIMUM_PLAYERS)
        maxPlayers = MAXIMUM_PLAYERS;
    if (maxPlayers < online)
        maxPlayers = online;
    if (maxPlayers < 1)
        maxPlayers = 1;

    mHandler.setMaxPlayers(maxPlayers);
    sender.sendTranslation("commands.setmaxplayers.success", {std::to_string(maxPlayers)});

    if (requested > MAXIMUM_PLAYERS)
        sender.sendTranslation("commands.setmaxplayers.success.upperbound", {});
    else if (requested < online)
        sender.sendTranslation("commands.setmaxplayers.success.lowerbound", {});

    return true;
}
