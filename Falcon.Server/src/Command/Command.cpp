#include "Command/Command.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

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

CommandParamData Command::makePlayerParameter(const std::string &name) {
    CommandParamData parameter;
    parameter.mName = name;
    parameter.mHasType = true;
    parameter.mType = CommandParamType::Target;

    return parameter;
}

bool Command::parseCoordinate(const std::string &value, float origin, float &out) {
    const bool relative = !value.empty() && value[0] == '~';
    const std::string number = relative ? value.substr(1) : value;

    float parsed = 0.0f;
    if (!number.empty()) {
        char *end = nullptr;
        errno = 0;
        parsed = std::strtof(number.c_str(), &end);
        if (end == number.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(parsed))
            return false;
    } else if (!relative) {
        return false;
    }

    out = relative ? origin + parsed : parsed;
    return true;
}

bool Command::parseBlockCoordinate(const std::string &value, int32_t origin, int32_t &out) {
    float parsed = 0.0f;
    if (!parseCoordinate(value, (float) origin, parsed))
        return false;

    const float floored = std::floor(parsed);
    if (floored < -30000000.0f || floored > 30000000.0f)
        return false;

    out = (int32_t) floored;
    return true;
}

bool Command::parseBlockPosition(const std::vector<std::string> &arguments, size_t first,
                                 const Vector3i &origin, Vector3i &out) {
    if (arguments.size() < first + 3)
        return false;

    return parseBlockCoordinate(arguments[first], origin.x, out.x)
           && parseBlockCoordinate(arguments[first + 1], origin.y, out.y)
           && parseBlockCoordinate(arguments[first + 2], origin.z, out.z);
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
