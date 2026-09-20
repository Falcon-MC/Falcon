#include "Command/ListCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <string>

ListCommand::ListCommand(ServerNetworkHandler &handler)
        : Command("list", "commands.list.description", "/list"), mHandler(handler) {}

std::vector<CommandOverloadData> ListCommand::getOverloads() const {
    return {CommandOverloadData()};
}

bool ListCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    (void) arguments;

    const std::vector<std::string> names = mHandler.getPlayerNames();

    sender.sendTranslation("commands.players.list",
                           {std::to_string(names.size()), std::to_string(mHandler.getMaxPlayers())});

    std::string joined;
    for (const std::string &name: names) {
        if (!joined.empty())
            joined += ", ";
        joined += name;
    }

    sender.sendMessage(joined);
    return true;
}
