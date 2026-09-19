#include "Command/KickCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

KickCommand::KickCommand(ServerNetworkHandler &handler)
        : Command("kick", "commands.kick.description", "/kick <player> [reason]"), mHandler(handler) {}

std::vector<CommandOverloadData> KickCommand::getOverloads() const {
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

bool KickCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    const std::string reason = joinArguments(arguments, 1);

    for (ServerPlayer *target: targets) {
        const std::string name = target->getName();
        mHandler._disconnect(target->getNetworkIdentifier(), reason.empty() ? "Kicked by admin" : reason);

        if (reason.empty())
            sender.sendTranslation("commands.kick.success", {name});
        else
            sender.sendTranslation("commands.kick.success.reason", {name, reason});
    }

    return true;
}
