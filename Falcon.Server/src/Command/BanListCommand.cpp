#include "Command/BanListCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/BanList.h"

BanListCommand::BanListCommand(ServerNetworkHandler &handler)
        : Command("banlist", "Lists the banned players or IP addresses", "/banlist [ips|players]"), mHandler(handler) {}

std::vector<CommandOverloadData> BanListCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeEnumParameter("type", "BanListType", {"ips", "players"}, true));
    return {overload};
}

bool BanListCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const bool ips = !arguments.empty() && arguments[0] == "ips";
    if (!arguments.empty() && !ips && arguments[0] != "players") {
        sender.sendTranslation("commands.generic.parameter.invalid", {arguments[0]});
        return false;
    }

    const std::vector<std::string> names = ips ? mHandler.getIpBanList().getNames()
                                               : mHandler.getBanList().getNames();

    std::string joined;
    for (const std::string &name: names) {
        if (!joined.empty())
            joined += ", ";
        joined += name;
    }

    sender.sendTranslation(ips ? "commands.banlist.ips" : "commands.banlist.players", {std::to_string(names.size())});
    if (!joined.empty())
        sender.sendMessage(joined);
    return true;
}
