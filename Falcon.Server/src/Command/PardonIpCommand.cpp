#include "Command/PardonIpCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/BanList.h"

PardonIpCommand::PardonIpCommand(ServerNetworkHandler &handler)
        : Command("pardon-ip", "Removes an IP address from the ban list", "/pardon-ip <address>", {"unban-ip"}),
          mHandler(handler) {}

std::vector<CommandOverloadData> PardonIpCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeTypedParameter("address", CommandParamType::String));
    return {overload};
}

bool PardonIpCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (!mHandler.getIpBanList().remove(arguments[0])) {
        sender.sendTranslation("commands.unbanip.invalid", {});
        return false;
    }

    sender.sendTranslation("commands.unbanip.success", {arguments[0]});
    return true;
}
