#include "Server/Commands/Command.h"

#include "Core/Math/MathConstants.h"
#include "World/Actor/Player/ServerPlayer.h"

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

CommandParamData Command::makeTypedParameter(const std::string &name, CommandParamType type, bool optional) {
    CommandParamData parameter;
    parameter.mName = name;
    parameter.mHasType = true;
    parameter.mType = type;
    parameter.mOptional = optional;

    return parameter;
}

CommandParamData Command::makeEnumParameter(const std::string &name, const std::string &enumName,
                                            const std::vector<std::string> &values, bool optional) {
    CommandParamData parameter;
    parameter.mName = name;
    parameter.mHasEnumData = true;
    parameter.mEnumData.mName = enumName;
    parameter.mEnumData.mValues = values;
    parameter.mOptional = optional;

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

bool Command::parsePosition(const std::vector<std::string> &arguments, size_t first, const Vector3f &origin,
                            const Vector3f &rotation, Vector3f &out) {
    if (arguments.size() < first + 3)
        return false;

    size_t local = 0;
    for (size_t index = first; index < first + 3; ++index)
        local += !arguments[index].empty() && arguments[index][0] == '^' ? 1 : 0;
    if (local == 0)
        return parseCoordinate(arguments[first], origin.x, out.x)
               && parseCoordinate(arguments[first + 1], origin.y, out.y)
               && parseCoordinate(arguments[first + 2], origin.z, out.z);
    // ^ cannot mix with world coordinates
    if (local != 3)
        return false;

    float offsets[3];
    for (size_t index = 0; index < 3; ++index) {
        const std::string number = arguments[first + index].substr(1);
        if (!parseCoordinate(number.empty() ? "~" : "~" + number, 0.0f, offsets[index]))
            return false;
    }

    // forward is the view vector, up the pitch less 90 degrees, left (cos yaw, 0, sin yaw) = up x forward
    const float pitch = rotation.x * MathConstants::DEGREES_TO_RADIANS_F;
    const float yaw = rotation.y * MathConstants::DEGREES_TO_RADIANS_F;
    const Vector3f forward(-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    const float upPitch = pitch - MathConstants::PI_F / 2.0f;
    const Vector3f up(-std::sin(yaw) * std::cos(upPitch), -std::sin(upPitch), std::cos(yaw) * std::cos(upPitch));
    // left = up x forward
    const Vector3f left(up.y * forward.z - up.z * forward.y, up.z * forward.x - up.x * forward.z,
                        up.x * forward.y - up.y * forward.x);
    out = Vector3f(origin.x + left.x * offsets[0] + up.x * offsets[1] + forward.x * offsets[2],
                   origin.y + left.y * offsets[0] + up.y * offsets[1] + forward.y * offsets[2],
                   origin.z + left.z * offsets[0] + up.z * offsets[1] + forward.z * offsets[2]);
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

bool Command::parseBlockPosition(const std::vector<std::string> &arguments, size_t first, const Vector3i &origin,
                                 Vector3i &out) {
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

std::string Command::joinPlayerNames(const std::vector<ServerPlayer *> &players) {
    std::string names;
    for (const ServerPlayer *player: players) {
        if (!names.empty())
            names += ", ";
        names += player->getName();
    }
    return names;
}
