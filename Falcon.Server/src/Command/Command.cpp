#include "Command/Command.h"

Command::Command(const std::string &name, const std::string &description, const std::string &usage,
                 const std::vector<std::string> &aliases)
        : mName(name), mDescription(description), mUsage(usage), mAliases(aliases) {}

std::vector<CommandOverloadData> Command::getOverloads() const {
    CommandParamData argsParameter;
    argsParameter.mName = "args";
    argsParameter.mOptional = true;
    argsParameter.mHasType = true;
    argsParameter.mType = CommandParamType::RawText;

    CommandOverloadData overload;
    overload.mParameters.push_back(argsParameter);

    return {overload};
}

CommandParamData Command::makePlayerParameter(const std::string &name,
                                              const std::vector<std::string> &playerNames) {
    CommandParamData parameter;
    parameter.mName = name;
    parameter.mHasEnumData = true;
    parameter.mEnumData.mName = "PlayerTarget";
    parameter.mEnumData.mIsSoft = true;
    parameter.mEnumData.mValues = {"@a", "@s", "@p", "@r"};

    for (const std::string &playerName: playerNames)
        parameter.mEnumData.mValues.push_back(playerName);

    return parameter;
}

std::string Command::joinArguments(const std::vector<std::string> &arguments, size_t first) {
    std::string joined;
    for (size_t index = first; index < arguments.size(); ++index) {
        if (!joined.empty())
            joined += " ";
        joined += arguments[index];
    }
    return joined;
}
