#include "Command/PlaySoundCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>

namespace {
    const float DEFAULT_VOLUME = 1.0f;
    const float DEFAULT_PITCH = 1.0f;
    const float HEARING_DISTANCE_PER_VOLUME = 16.0f;

    bool parseFloat(const std::string &value, float &out) {
        return Command::parseCoordinate(value, 0.0f, out) && value[0] != '~';
    }
}

PlaySoundCommand::PlaySoundCommand(ServerNetworkHandler &handler)
        : Command("playsound", "commands.playsound.description",
                  "/playsound <sound> [player] [position] [volume] [pitch] [minimumVolume]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> PlaySoundCommand::getOverloads() const {
    CommandParamData player = makePlayerParameter("player");
    player.mOptional = true;

    CommandOverloadData overload;
    overload.mParameters = {makeTypedParameter("sound", CommandParamType::String), player,
                            makeTypedParameter("position", CommandParamType::Position, true),
                            makeTypedParameter("volume", CommandParamType::Float, true),
                            makeTypedParameter("pitch", CommandParamType::Float, true),
                            makeTypedParameter("minimumVolume", CommandParamType::Float, true)};
    return {overload};
}

bool PlaySoundCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    std::vector<ServerPlayer *> targets;
    if (arguments.size() > 1) {
        targets = mHandler.resolveTargets(sender, arguments[1]);
    } else if (ServerPlayer *self = sender.asPlayer()) {
        targets.push_back(self);
    }

    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    float volume = DEFAULT_VOLUME;
    float pitch = DEFAULT_PITCH;
    float minimumVolume = 0.0f;
    if ((arguments.size() > 5 && !parseFloat(arguments[5], volume))
        || (arguments.size() > 6 && !parseFloat(arguments[6], pitch))
        || (arguments.size() > 7 && !parseFloat(arguments[7], minimumVolume))) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    int played = 0;
    std::string lastName;
    for (ServerPlayer *target: targets) {
        const Vector3f origin = target->getPosition();
        Vector3f position = origin;

        if (arguments.size() > 4
            && (!parseCoordinate(arguments[2], origin.x, position.x)
                || !parseCoordinate(arguments[3], origin.y, position.y)
                || !parseCoordinate(arguments[4], origin.z, position.z))) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }

        const float dx = position.x - origin.x;
        const float dy = position.y - origin.y;
        const float dz = position.z - origin.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float range = std::max(volume, 1.0f) * HEARING_DISTANCE_PER_VOLUME;

        if (distance > range) {
            if (minimumVolume <= 0.0f) {
                sender.sendTranslation("commands.playsound.playerTooFar", {target->getName()});
                continue;
            }

            position = origin;
            volume = minimumVolume;
        }

        mHandler.playSoundFor(*target, arguments[0], position, volume, pitch);
        lastName = target->getName();
        ++played;
    }

    if (played == 0)
        return false;

    if (played == 1)
        sender.sendTranslation("commands.playsound.success.single", {arguments[0], lastName});
    else
        sender.sendTranslation("commands.playsound.success.multiple", {arguments[0], std::to_string(played)});
    return true;
}
