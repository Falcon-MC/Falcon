#include "Command/BanCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

BanCommand::BanCommand(ServerNetworkHandler &handler)
        : Command("ban", "commands.ban.description", "/ban <player> [reason]"), mHandler(handler) {}

std::vector<CommandOverloadData> BanCommand::getOverloads() const {
    CommandParamData reasonParameter;
    reasonParameter.mName = "reason";
    reasonParameter.mOptional = true;
    reasonParameter.mHasType = true;
    reasonParameter.mType = CommandParamType::Message;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("player", mHandler.getPlayerNames()));
    overload.mParameters.push_back(reasonParameter);
    return {overload};
}

bool BanCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string &name = arguments[0];
    const std::string reason = joinArguments(arguments, 1);
    mHandler.getBanList().add(name, reason, sender.getSenderName());

    ServerPlayer *target = mHandler.getPlayerByName(name);
    if (target != nullptr) {
        mHandler._disconnect(target->getNetworkIdentifier(),
                             reason.empty() ? "Banned by admin" : "Banned by admin. Reason: " + reason);
    }

    sender.sendTranslation("commands.ban.success", {target != nullptr ? target->getName() : name});
    return true;
}
