#include "Command/GameRuleCommand.h"

#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    std::string valueOf(const GameRules::Rule &rule) {
        if (rule.mType == GameRuleData::Type::Bool)
            return rule.mBoolValue ? "true" : "false";

        return std::to_string(rule.mIntValue);
    }
}

GameRuleCommand::GameRuleCommand(ServerNetworkHandler &handler)
        : Command("gamerule", "commands.gamerule.description", "/gamerule <rule> [value]"), mHandler(handler) {}

CommandPermission GameRuleCommand::getRequiredPermission() const {
    return CommandPermission::GameDirectors;
}

std::vector<CommandOverloadData> GameRuleCommand::getOverloads() const {
    CommandParamData ruleParameter;
    ruleParameter.mName = "rule";
    ruleParameter.mHasEnumData = true;
    ruleParameter.mEnumData.mName = "GameRule";
    ruleParameter.mEnumData.mIsSoft = false;
    ruleParameter.mEnumData.mValues = mHandler.getLevel().getGameRules().getNames();

    CommandParamData valueParameter;
    valueParameter.mName = "value";
    valueParameter.mOptional = true;

    CommandOverloadData overload;
    overload.mParameters.push_back(ruleParameter);
    overload.mParameters.push_back(valueParameter);

    return {overload};
}

bool GameRuleCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    GameRules &rules = mHandler.getLevel().getGameRules();

    if (arguments.empty()) {
        std::string names;
        for (const std::string &name: rules.getNames()) {
            if (!names.empty())
                names += ", ";
            names += name;
        }

        sender.sendMessage(names);
        return true;
    }

    const GameRules::Rule *rule = rules.find(arguments[0]);
    if (rule == nullptr) {
        sender.sendTranslation("commands.generic.parameter.invalid", {arguments[0]});
        return false;
    }

    if (arguments.size() < 2) {
        sender.sendMessage(rule->mName + " = " + valueOf(*rule));
        return true;
    }

    if (!mHandler.changeGameRule(arguments[0], arguments[1])) {
        sender.sendTranslation("commands.generic.parameter.invalid", {arguments[1]});
        return false;
    }

    rule = rules.find(arguments[0]);
    sender.sendTranslation("commands.gamerule.success", {rule->mName, valueOf(*rule)});
    return true;
}
