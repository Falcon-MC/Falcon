#include "Command/MeCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

MeCommand::MeCommand(ServerNetworkHandler &handler)
        : Command("me", "commands.me.description", "/me <action>"), mHandler(handler) {}

CommandPermission MeCommand::getRequiredPermission() const {
    return CommandPermission::Any;
}

std::vector<CommandOverloadData> MeCommand::getOverloads() const {
    CommandParamData actionParameter;
    actionParameter.mName = "action";
    actionParameter.mHasType = true;
    actionParameter.mType = CommandParamType::Message;

    CommandOverloadData overload;
    overload.mParameters.push_back(actionParameter);

    return {overload};
}

bool MeCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string action = joinArguments(arguments, 0);
    if (action.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string senderName = sender.getSenderName();

    mHandler.broadcastTranslation("chat.type.emote", {senderName, action});

    if (!sender.isPlayer())
        sender.sendTranslation("chat.type.emote", {senderName, action});

    return true;
}
