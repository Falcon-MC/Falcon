#include "Server/Commands/AimAssistCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CameraAimAssistPacket.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <cmath>
#include <cstdlib>

namespace {
    // defaults from client 0x1485B94B0, limits from its execute 0x14857B770
    const float DEFAULT_ANGLE = 50.0f;
    const float DEFAULT_DISTANCE = 8.5f;
    const float MIN_ANGLE = 10.0f;
    const float MAX_ANGLE = 90.0f;
    const float MIN_DISTANCE = 1.0f;
    const float MAX_DISTANCE = 16.0f;
    const char *const DEFAULT_PRESET = "minecraft:aim_assist_default";

    bool parseFloat(const std::string &value, float &out) {
        char *end = nullptr;
        const float parsed = std::strtof(value.c_str(), &end);
        if (value.empty() || *end != '\0' || !std::isfinite(parsed))
            return false;

        out = parsed;
        return true;
    }
}

AimAssistCommand::AimAssistCommand(ServerNetworkHandler &handler)
        : Command("aimassist", "commands.aimassist.description",
                  "/aimassist <player> set [x_angle] [y_angle] [max_distance] [angle|distance] [preset] | "
                  "/aimassist <player> clear"),
          mHandler(handler) {}

std::vector<CommandOverloadData> AimAssistCommand::getOverloads() const {
    CommandOverloadData set;
    set.mParameters.push_back(makePlayerParameter("player"));
    set.mParameters.push_back(makeEnumParameter("set", "AimAssistSet", {"set"}));
    set.mParameters.push_back(makeTypedParameter("x_angle", CommandParamType::Float, true));
    set.mParameters.push_back(makeTypedParameter("y_angle", CommandParamType::Float, true));
    set.mParameters.push_back(makeTypedParameter("max_distance", CommandParamType::Float, true));
    set.mParameters.push_back(makeEnumParameter("target_mode", "AimAssistTargetMode", {"angle", "distance"}, true));
    set.mParameters.push_back(makeTypedParameter("preset_id", CommandParamType::String, true));

    CommandOverloadData clear;
    clear.mParameters.push_back(makePlayerParameter("player"));
    clear.mParameters.push_back(makeEnumParameter("clear", "AimAssistClear", {"clear"}));
    return {set, clear};
}

bool AimAssistCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2 || (arguments[1] != "set" && arguments[1] != "clear")) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    CameraAimAssistPacket packet;
    packet.mViewAngle.x = DEFAULT_ANGLE;
    packet.mViewAngle.y = DEFAULT_ANGLE;
    packet.mDistance = DEFAULT_DISTANCE;
    packet.mTargetMode = CameraAimAssistPacket::TargetMode::Angle;
    packet.mPresetId = DEFAULT_PRESET;
    packet.mAction = arguments[1] == "set" ? AimAssistAction::Set : AimAssistAction::Clear;

    if (packet.mAction == AimAssistAction::Set) {
        const bool numbers = (arguments.size() < 3 || parseFloat(arguments[2], packet.mViewAngle.x))
                             && (arguments.size() < 4 || parseFloat(arguments[3], packet.mViewAngle.y))
                             && (arguments.size() < 5 || parseFloat(arguments[4], packet.mDistance));
        if (!numbers || (arguments.size() >= 6 && arguments[5] != "angle" && arguments[5] != "distance")) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        if (packet.mViewAngle.x < MIN_ANGLE || packet.mViewAngle.x > MAX_ANGLE) {
            sender.sendTranslation("commands.aimassist.invalidxangle", {});
            return false;
        }
        if (packet.mViewAngle.y < MIN_ANGLE || packet.mViewAngle.y > MAX_ANGLE) {
            sender.sendTranslation("commands.aimassist.invalidyangle", {});
            return false;
        }
        if (packet.mDistance < MIN_DISTANCE || packet.mDistance > MAX_DISTANCE) {
            sender.sendTranslation("commands.aimassist.invaliddistance", {});
            return false;
        }
        if (arguments.size() >= 6 && arguments[5] == "distance")
            packet.mTargetMode = CameraAimAssistPacket::TargetMode::Distance;
        if (arguments.size() >= 7)
            packet.mPresetId = arguments[6];
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    for (ServerPlayer *target: targets)
        mHandler.sendPacketTo(target->getNetworkIdentifier(), packet);

    sender.sendTranslation("commands.aimassist.success", {joinPlayerNames(targets)});
    return true;
}
