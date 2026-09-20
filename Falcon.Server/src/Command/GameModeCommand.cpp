#include "Command/GameModeCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cctype>

namespace {
    std::string toLowerCase(const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return lowered;
    }
}

GameModeCommand::GameModeCommand(ServerNetworkHandler &handler)
        : Command("gamemode", "commands.gamemode.description", "/gamemode <mode> [player]", {"gm"}),
          mHandler(handler) {}

std::vector<CommandOverloadData> GameModeCommand::getOverloads() const {
    CommandParamData playerParameter = makePlayerParameter("player");
    playerParameter.mOptional = true;

    CommandParamData namedMode;
    namedMode.mName = "gameMode";
    namedMode.mHasEnumData = true;
    namedMode.mEnumData.mName = "GameMode";
    namedMode.mEnumData.mValues = {"survival", "creative", "adventure", "spectator", "s", "c", "a"};

    CommandParamData numericMode;
    numericMode.mName = "gameMode";
    numericMode.mHasType = true;
    numericMode.mType = CommandParamType::Int;

    CommandOverloadData byName;
    byName.mParameters = {namedMode, playerParameter};

    CommandOverloadData byNumber;
    byNumber.mParameters = {numericMode, playerParameter};

    return {byName, byNumber};
}

int GameModeCommand::parseGameMode(const std::string &value) {
    const std::string mode = toLowerCase(value);

    if (mode == "0" || mode == "survival" || mode == "s")
        return 0;
    if (mode == "1" || mode == "creative" || mode == "c")
        return 1;
    if (mode == "2" || mode == "adventure" || mode == "a")
        return 2;
    if (mode == "3" || mode == "6" || mode == "spectator" || mode == "spc" || mode == "view" || mode == "v")
        return (int) GameType::Spectator;

    return -1;
}

const char *GameModeCommand::getGameModeName(int gameMode) {
    // The client resolves a leading % inside a translation parameter, so the mode name gets
    // localised the same way the vanilla command does it.
    switch ((GameType) gameMode) {
        case GameType::Survival:
            return "%gameMode.survival";
        case GameType::Creative:
            return "%gameMode.creative";
        case GameType::Adventure:
            return "%gameMode.adventure";
        case GameType::Spectator:
            return "%gameMode.spectator";
        default:
            return "Unknown";
    }
}

bool GameModeCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const int gameMode = parseGameMode(arguments[0]);
    if (gameMode < 0) {
        sender.sendTranslation("commands.gamemode.fail.invalid", {arguments[0]});
        return false;
    }

    std::vector<ServerPlayer *> targets;

    if (arguments.size() > 1) {
        targets = mHandler.resolveTargets(sender, arguments[1]);
        if (targets.empty()) {
            sender.sendTranslation("commands.generic.noTargetMatch", {});
            return false;
        }
    } else {
        ServerPlayer *self = sender.asPlayer();
        if (self == nullptr) {
            sender.sendTranslation("commands.generic.targetNotPlayer", {});
            return false;
        }

        targets.push_back(self);
    }

    const std::string modeName = getGameModeName(gameMode);

    for (ServerPlayer *target: targets) {
        mHandler.setPlayerGameMode(*target, gameMode);

        if (target == sender.asPlayer())
            sender.sendTranslation("commands.gamemode.success.self", {modeName});
        else
            sender.sendTranslation("commands.gamemode.success.other", {target->getName(), modeName});
    }

    return true;
}
