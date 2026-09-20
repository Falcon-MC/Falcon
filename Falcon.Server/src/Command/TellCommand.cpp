#include "Command/TellCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

TellCommand::TellCommand(ServerNetworkHandler &handler)
        : Command("tell", "commands.tell.description", "/tell <target> <message>", {"msg", "w"}),
          mHandler(handler) {}

CommandPermission TellCommand::getRequiredPermission() const {
    return CommandPermission::Any;
}

std::vector<CommandOverloadData> TellCommand::getOverloads() const {
    CommandParamData messageParameter;
    messageParameter.mName = "message";
    messageParameter.mHasType = true;
    messageParameter.mType = CommandParamType::Message;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("target", mHandler.getPlayerNames()));
    overload.mParameters.push_back(messageParameter);

    return {overload};
}

bool TellCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string message = joinArguments(arguments, 1);
    if (message.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    ServerPlayer *self = sender.asPlayer();
    const std::string senderName = sender.getSenderName();

    bool delivered = false;
    for (ServerPlayer *target: targets) {
        if (self != nullptr && target == self) {
            sender.sendTranslation("commands.message.sameTarget", {});
            continue;
        }

        target->sendTranslation("commands.message.display.incoming", {senderName, message});
        sender.sendTranslation("commands.message.display.outgoing", {target->getName(), message});
        delivered = true;
    }

    return delivered;
}
