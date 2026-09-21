#include "Command/DefaultGameModeCommand.h"

#include "Command/GameModeCommand.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

DefaultGameModeCommand::DefaultGameModeCommand(ServerNetworkHandler &handler)
        : Command("defaultgamemode", "commands.defaultgamemode.description", "/defaultgamemode <mode>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> DefaultGameModeCommand::getOverloads() const {
    CommandOverloadData byName;
    byName.mParameters.push_back(makeEnumParameter("gameMode", "GameMode",
                                                   {"survival", "creative", "adventure", "spectator", "s", "c",
                                                    "a"}));

    CommandOverloadData byNumber;
    byNumber.mParameters.push_back(makeTypedParameter("gameMode", CommandParamType::Int));

    return {byName, byNumber};
}

bool DefaultGameModeCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const int gameMode = GameModeCommand::parseGameMode(arguments[0]);
    if (gameMode < 0) {
        sender.sendTranslation("commands.generic.parameter.invalid", {arguments[0]});
        return false;
    }

    mHandler.setDefaultGameType((GameType) gameMode);
    sender.sendTranslation("commands.defaultgamemode.success", {GameModeCommand::getGameModeName(gameMode)});
    return true;
}
