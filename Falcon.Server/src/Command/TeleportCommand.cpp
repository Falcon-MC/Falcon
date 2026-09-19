#include "Command/TeleportCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
    std::string formatCoordinate(float value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", (double) value);
        return buffer;
    }

    CommandParamData makePositionParameter() {
        CommandParamData parameter;
        parameter.mName = "destination";
        parameter.mHasType = true;
        parameter.mType = CommandParamType::Position;
        return parameter;
    }
}

TeleportCommand::TeleportCommand(ServerNetworkHandler &handler)
        : Command("tp", "commands.tp.description", "/tp [victim] <destination|x y z>", {"teleport"}),
          mHandler(handler) {}

std::vector<CommandOverloadData> TeleportCommand::getOverloads() const {
    const std::vector<std::string> names = mHandler.getPlayerNames();

    CommandOverloadData toPlayer;
    toPlayer.mParameters.push_back(makePlayerParameter("destination", names));

    CommandOverloadData toPosition;
    toPosition.mParameters.push_back(makePositionParameter());

    CommandOverloadData victimToPlayer;
    victimToPlayer.mParameters.push_back(makePlayerParameter("victim", names));
    victimToPlayer.mParameters.push_back(makePlayerParameter("destination", names));

    CommandOverloadData victimToPosition;
    victimToPosition.mParameters.push_back(makePlayerParameter("victim", names));
    victimToPosition.mParameters.push_back(makePositionParameter());

    return {toPlayer, toPosition, victimToPlayer, victimToPosition};
}

bool TeleportCommand::parseCoordinate(const std::string &value, float origin, float &out) {
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

bool TeleportCommand::parsePosition(const std::vector<std::string> &arguments, size_t first,
                                    const Vector3f &origin, Vector3f &out) {
    if (arguments.size() != first + 3)
        return false;

    return parseCoordinate(arguments[first], origin.x, out.x)
           && parseCoordinate(arguments[first + 1], origin.y, out.y)
           && parseCoordinate(arguments[first + 2], origin.z, out.z);
}

bool TeleportCommand::teleportToPlayer(CommandOrigin &sender, const std::vector<ServerPlayer *> &victims,
                                       const std::string &destination) {
    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, destination);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }
    if (targets.size() > 1) {
        sender.sendTranslation("commands.generic.tooManyTargets", {});
        return false;
    }

    ServerPlayer &target = *targets.front();
    for (ServerPlayer *victim: victims) {
        if (victim->getDimension() != target.getDimension())
            mHandler.changePlayerDimension(*victim, target.getDimension(), target.getPosition());
        else
            victim->teleport(mHandler, target.getPosition());

        sender.sendTranslation("commands.tp.success", {victim->getName(), target.getName()});
    }

    return true;
}

bool TeleportCommand::teleportToPosition(CommandOrigin &sender, const std::vector<ServerPlayer *> &victims,
                                         const std::vector<std::string> &arguments, size_t first) {
    for (ServerPlayer *victim: victims) {
        Vector3f destination;
        if (!parsePosition(arguments, first, victim->getPosition(), destination)) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        victim->teleport(mHandler, destination);
        sender.sendTranslation("commands.tp.success.coordinates",
                               {victim->getName(), formatCoordinate(destination.x), formatCoordinate(destination.y),
                                formatCoordinate(destination.z)});
    }

    return true;
}

bool TeleportCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() == 1 || arguments.size() == 3) {
        ServerPlayer *self = sender.asPlayer();
        if (self == nullptr) {
            sender.sendTranslation("commands.generic.targetNotPlayer", {});
            return false;
        }

        if (arguments.size() == 1)
            return teleportToPlayer(sender, {self}, arguments[0]);

        return teleportToPosition(sender, {self}, arguments, 0);
    }

    if (arguments.size() == 2 || arguments.size() == 4) {
        const std::vector<ServerPlayer *> victims = mHandler.resolveTargets(sender, arguments[0]);
        if (victims.empty()) {
            sender.sendTranslation("commands.generic.noTargetMatch", {});
            return false;
        }

        if (arguments.size() == 2)
            return teleportToPlayer(sender, victims, arguments[1]);

        return teleportToPosition(sender, victims, arguments, 1);
    }

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}
