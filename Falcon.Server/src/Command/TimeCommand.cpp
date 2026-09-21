#include "Command/TimeCommand.h"

#include "Network/Handler/ServerNetworkHandler.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>

TimeCommand::TimeCommand(ServerNetworkHandler &handler)
        : Command("time", "commands.time.description", "/time <add|set|query|start|stop> ..."),
          mHandler(handler) {}

bool TimeCommand::parseInteger(const std::string &value, int64_t &out) {
    if (value.empty())
        return false;
    char *end = nullptr;
    errno = 0;
    const long long parsed = std::strtoll(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || errno == ERANGE)
        return false;
    out = (int64_t) parsed;
    return true;
}

bool TimeCommand::parseTime(const std::string &value, int64_t &out) {
    std::string lowered = value;
    for (char &character: lowered)
        character = (char) std::tolower((unsigned char) character);

    if (lowered == "day") {
        out = 1000;
        return true;
    }
    if (lowered == "noon") {
        out = 6000;
        return true;
    }
    if (lowered == "sunset") {
        out = 12000;
        return true;
    }
    if (lowered == "night") {
        out = 13000;
        return true;
    }
    if (lowered == "midnight") {
        out = 18000;
        return true;
    }
    if (lowered == "sunrise") {
        out = 23000;
        return true;
    }
    return parseInteger(value, out);
}

std::vector<CommandOverloadData> TimeCommand::getOverloads() const {
    const auto enumParameter = [](const std::string &name, const std::string &enumName,
                                  const std::vector<std::string> &values) {
        CommandParamData parameter;
        parameter.mName = name;
        parameter.mHasEnumData = true;
        parameter.mEnumData.mName = enumName;
        parameter.mEnumData.mValues = values;
        return parameter;
    };

    CommandParamData amount;
    amount.mName = "amount";
    amount.mHasType = true;
    amount.mType = CommandParamType::Int;

    CommandOverloadData add;
    add.mParameters = {enumParameter("mode", "TimeModeAdd", {"add"}), amount};

    CommandOverloadData setAmount;
    setAmount.mParameters = {enumParameter("mode", "TimeModeSet", {"set"}), amount};

    CommandOverloadData setTime;
    setTime.mParameters = {enumParameter("mode", "TimeModeSet", {"set"}),
                           enumParameter("time", "TimeSpec",
                                         {"day", "night", "midnight", "noon", "sunrise", "sunset"})};

    CommandOverloadData query;
    query.mParameters = {enumParameter("mode", "TimeModeQuery", {"query"}),
                         enumParameter("query", "TimeQuery", {"daytime", "gametime", "day"})};

    CommandOverloadData daylightCycle;
    daylightCycle.mParameters = {enumParameter("mode", "TimeMode", {"start", "stop"})};

    return {add, setAmount, setTime, query, daylightCycle};
}

bool TimeCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    std::string operation = arguments[0];
    for (char &character: operation)
        character = (char) std::tolower((unsigned char) character);

    if (operation == "start" || operation == "stop") {
        const bool running = operation == "start";
        mHandler.changeGameRule("dodaylightcycle", running ? "true" : "false");
        sender.sendTranslation("commands.gamerule.success", {"dodaylightcycle", running ? "true" : "false"});
        return true;
    }

    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (operation == "set") {
        int64_t time = 0;
        if (!parseTime(arguments[1], time) || time < 0) {
            sender.sendTranslation("commands.generic.syntax", {"/time set ", arguments[1], ""});
            return false;
        }
        mHandler.getLevel().setTime(time);
        mHandler.broadcastWorldTime();
        sender.sendTranslation("commands.time.set", {std::to_string(mHandler.getLevel().getDayTime())});
        return true;
    }

    if (operation == "add") {
        int64_t time = 0;
        if (!parseInteger(arguments[1], time)) {
            sender.sendTranslation("commands.generic.syntax", {"/time add ", arguments[1], ""});
            return false;
        }
        mHandler.getLevel().addTime(time);
        mHandler.broadcastWorldTime();
        sender.sendTranslation("commands.time.added", {std::to_string(time)});
        return true;
    }

    if (operation == "query") {
        const std::string query = arguments[1];
        if (query == "daytime")
            sender.sendTranslation("commands.time.query.daytime", {std::to_string(mHandler.getLevel().getDayTime())});
        else if (query == "gametime")
            sender.sendTranslation("commands.time.query.gametime", {std::to_string(mHandler.getLevel().getTime())});
        else if (query == "day")
            sender.sendTranslation("commands.time.query.day", {std::to_string(mHandler.getLevel().getTime() / 24000)});
        else {
            sender.sendTranslation("commands.generic.syntax", {"/time query ", query, ""});
            return false;
        }
        return true;
    }

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}
