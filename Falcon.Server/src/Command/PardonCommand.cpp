#include "Command/PardonCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

PardonCommand::PardonCommand(ServerNetworkHandler &handler)
        : Command("pardon", "Removes a player from the ban list", "/pardon <player>", {"unban"}), mHandler(handler) {}

std::vector<CommandOverloadData> PardonCommand::getOverloads() const {
    CommandParamData playerParameter;
    playerParameter.mName = "player";
    playerParameter.mType = CommandParamType::Target;

    CommandOverloadData overload;
    overload.mParameters.push_back(playerParameter);
    return {overload};
}

bool PardonCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (!mHandler.getBanList().remove(arguments[0])) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    sender.sendTranslation("commands.unban.success", {arguments[0]});
    return true;
}
