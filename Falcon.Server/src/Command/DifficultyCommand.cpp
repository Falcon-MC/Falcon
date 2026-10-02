#include "Command/DifficultyCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/SetDifficultyPacket.h"

#include <algorithm>
#include <string>

namespace {
    const std::vector<std::string> DIFFICULTY_NAMES = {"peaceful", "easy", "normal", "hard"};

    std::string toLower(const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return lowered;
    }
}

DifficultyCommand::DifficultyCommand(ServerNetworkHandler &handler)
        : Command("difficulty", "commands.difficulty.description", "/difficulty <peaceful|easy|normal|hard>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> DifficultyCommand::getOverloads() const {
    CommandParamData difficulty;
    difficulty.mName = "difficulty";
    difficulty.mHasEnumData = true;
    difficulty.mEnumData.mName = "Difficulty";
    difficulty.mEnumData.mValues = DIFFICULTY_NAMES;

    CommandOverloadData overload;
    overload.mParameters.push_back(difficulty);

    return {overload};
}

bool DifficultyCommand::parseDifficulty(const std::string &value, Difficulty &out) {
    const std::string lowered = toLower(value);

    for (size_t index = 0; index < DIFFICULTY_NAMES.size(); ++index) {
        if (lowered == DIFFICULTY_NAMES[index] || lowered == std::to_string(index)
            || (lowered.size() == 1 && lowered[0] == DIFFICULTY_NAMES[index][0])) {
            out = (Difficulty) index;
            return true;
        }
    }

    return false;
}

bool DifficultyCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() != 1) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    Difficulty difficulty = Difficulty::Normal;
    if (!parseDifficulty(arguments[0], difficulty)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    mHandler.setDifficulty(DIFFICULTY_NAMES[(size_t) difficulty]);

    SetDifficultyPacket packet;
    packet.mDifficulty = (uint32_t) difficulty;

    for (auto &entry: mHandler.getPlayers()) {
        if (entry.second.isSpawned())
            mHandler.getNetworkHandler().send(entry.first, packet, mHandler.getCodecContext());
    }

    sender.sendTranslation("commands.difficulty.success", {DIFFICULTY_NAMES[(size_t) difficulty]});
    return true;
}
