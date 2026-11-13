#include "Server/Commands/TellCommand.h"

#include "Core/Event/EventBus.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"

TellCommand::TellCommand(ServerNetworkHandler &handler)
        : Command("tell", "commands.tell.description", "/tell <target> <message>", {"msg", "w"}), mHandler(handler) {}

CommandPermission TellCommand::getRequiredPermission() const {
    return CommandPermission::Any;
}

std::vector<CommandOverloadData> TellCommand::getOverloads() const {
    CommandParamData messageParameter;
    messageParameter.mName = "message";
    messageParameter.mHasType = true;
    messageParameter.mType = CommandParamType::Message;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("target"));
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

    // a player's message goes through the chat event with its targets, and stops if cancelled
    if (self != nullptr) {
        PlayerChatBeforeEvent before(*self, message);
        before.mTargets = &targets;
        mHandler.getEventBus().before().mChatSend.emit(before);
        if (before.isCancelled()) {
            sender.sendTranslation("commands.generic.canceled", {});
            return false;
        }
    }

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

    if (self != nullptr) {
        PlayerChatAfterEvent after(*self, message);
        after.mTargets = &targets;
        mHandler.getEventBus().after().mChatSend.emit(after);
    }
    return delivered;
}
