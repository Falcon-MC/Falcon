#include "Command/DayLockCommand.h"

#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace {
    const int64_t LOCKED_DAY_TIME = 5000;
}

DayLockCommand::DayLockCommand(ServerNetworkHandler &handler)
        : Command("daylock", "commands.daylock.description", "/daylock [lock]", {"alwaysday"}), mHandler(handler) {}

std::vector<CommandOverloadData> DayLockCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeEnumParameter("lock", "Boolean", {"true", "false"}, true));
    return {overload};
}

bool DayLockCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    bool lock = true;
    if (!arguments.empty()) {
        if (arguments[0] != "true" && arguments[0] != "false") {
            sender.sendTranslation("commands.generic.boolean.invalid", {arguments[0]});
            return false;
        }
        lock = arguments[0] == "true";
    }

    mHandler.changeGameRule("dodaylightcycle", lock ? "false" : "true");

    if (lock) {
        Level &level = mHandler.getLevel();
        level.setTime(level.getTime() - level.getDayTime() + LOCKED_DAY_TIME);
        mHandler.broadcastWorldTime();
    }

    sender.sendTranslation(lock ? "commands.always.day.locked" : "commands.always.day.unlocked", {});
    return true;
}
