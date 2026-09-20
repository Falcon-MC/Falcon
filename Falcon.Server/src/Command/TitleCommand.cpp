#include "Command/TitleCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <string>

namespace {
    const std::vector<std::string> ACTIONS = {"clear", "reset", "title", "subtitle", "actionbar", "times"};

    bool parseTicks(const std::string &value, int32_t &out) {
        char *end = nullptr;
        errno = 0;
        const long parsed = std::strtol(value.c_str(), &end, 10);

        if (value.empty() || end == value.c_str() || *end != '\0' || errno == ERANGE || parsed < 0)
            return false;

        out = (int32_t) parsed;
        return true;
    }
}

TitleCommand::TitleCommand(ServerNetworkHandler &handler, bool raw)
        : Command(raw ? "titleraw" : "title", "commands.title.description",
                  raw ? "/titleraw <player> <clear|reset|title|subtitle|actionbar|times> [json]"
                      : "/title <player> <clear|reset|title|subtitle|actionbar|times> [text|fadeIn stay fadeOut]"),
          mHandler(handler), mRaw(raw) {}

std::vector<CommandOverloadData> TitleCommand::getOverloads() const {
    CommandParamData action;
    action.mName = "action";
    action.mHasEnumData = true;
    action.mEnumData.mName = "TitleAction";
    action.mEnumData.mValues = ACTIONS;

    CommandParamData text;
    text.mName = "text";
    text.mOptional = true;
    text.mHasType = true;
    text.mType = CommandParamType::Message;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("player"));
    overload.mParameters.push_back(action);
    overload.mParameters.push_back(text);

    return {overload};
}

bool TitleCommand::applyToTarget(ServerPlayer &target, const std::string &action,
                                 const std::vector<std::string> &arguments) {
    if (action == "clear") {
        target.clearTitle();
        return true;
    }

    if (action == "reset") {
        target.resetTitle();
        return true;
    }

    if (action == "times") {
        if (arguments.size() != 5)
            return false;

        int32_t fadeInTime = 0;
        int32_t stayTime = 0;
        int32_t fadeOutTime = 0;

        if (!parseTicks(arguments[2], fadeInTime) || !parseTicks(arguments[3], stayTime)
            || !parseTicks(arguments[4], fadeOutTime))
            return false;

        target.setTitleTimes(fadeInTime, stayTime, fadeOutTime);
        return true;
    }

    if (arguments.size() < 3)
        return false;

    const std::string text = joinArguments(arguments, 2);

    if (action == "title")
        target.sendTitleText(text, mRaw);
    else if (action == "subtitle")
        target.sendSubtitle(text, mRaw);
    else
        target.sendActionBar(text, mRaw);

    return true;
}

bool TitleCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::string action = arguments[1];
    if (std::find(ACTIONS.begin(), ACTIONS.end(), action) == ACTIONS.end()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    for (ServerPlayer *target: targets) {
        if (!applyToTarget(*target, action, arguments)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
    }

    sender.sendTranslation("commands.title.success", {});
    return true;
}
