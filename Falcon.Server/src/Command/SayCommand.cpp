#include "Command/SayCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

SayCommand::SayCommand(ServerNetworkHandler &handler)
        : Command("say", "commands.say.description", "/say <message>"), mHandler(handler) {}

std::vector<CommandOverloadData> SayCommand::getOverloads() const {
    CommandParamData messageParameter;
    messageParameter.mName = "message";
    messageParameter.mHasType = true;
    messageParameter.mType = CommandParamType::Message;

    CommandOverloadData overload;
    overload.mParameters.push_back(messageParameter);

    return {overload};
}

bool SayCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string message = joinArguments(arguments, 0);
    if (message.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string senderName = sender.getSenderName();

    mHandler.broadcastTranslation("chat.type.announcement", {senderName, message});

    if (!sender.isPlayer())
        sender.sendTranslation("chat.type.announcement", {senderName, message});

    return true;
}
