#include "Server/Commands/ScriptEventCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Player/ServerPlayer.h"

ScriptEventCommand::ScriptEventCommand(ServerNetworkHandler &handler)
        : Command("scriptevent", "commands.scriptevent.description", "/scriptevent <messageId> [message]"),
          mHandler(handler) {}

CommandPermission ScriptEventCommand::getRequiredPermission() const {
    return CommandPermission::GameDirectors;
}

std::vector<CommandOverloadData> ScriptEventCommand::getOverloads() const {
    CommandParamData idParameter;
    idParameter.mName = "messageId";
    idParameter.mHasType = true;
    idParameter.mType = CommandParamType::String;

    CommandParamData messageParameter;
    messageParameter.mName = "message";
    messageParameter.mHasType = true;
    messageParameter.mType = CommandParamType::Message;
    messageParameter.mOptional = true;

    CommandOverloadData overload;
    overload.mParameters.push_back(idParameter);
    overload.mParameters.push_back(messageParameter);

    return {overload};
}

bool ScriptEventCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string &id = arguments[0];
    const size_t separator = id.find(':');
    if (separator == std::string::npos || separator == 0 || separator + 1 == id.size()
        || id.compare(0, separator, "minecraft") == 0) {
        sender.sendTranslation("commands.scriptevent.invalidNamespace", {});
        return false;
    }

    ScriptEventOrigin origin;
    ServerPlayer *player = sender.asPlayer();
    if (player != nullptr) {
        origin.mSourceType = "Entity";
        origin.mSourceEntity = player;
    }

    mHandler.getScriptEngine().emitScriptEvent(id, joinArguments(arguments, 1), origin);
    return true;
}
