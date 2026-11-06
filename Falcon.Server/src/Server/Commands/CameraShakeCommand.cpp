#include "Server/Commands/CameraShakeCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CameraShakePacket.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <cmath>
#include <cstdlib>

namespace {
    // command defaults from client 0x1485BBD20
    const float DEFAULT_INTENSITY = 0.5f;
    const float DEFAULT_SECONDS = 1.0f;

    bool parseFloat(const std::string &value, float &out) {
        char *end = nullptr;
        const float parsed = std::strtof(value.c_str(), &end);
        if (value.empty() || *end != '\0' || !std::isfinite(parsed))
            return false;

        out = parsed;
        return true;
    }

    std::string joinNames(const std::vector<ServerPlayer *> &players) {
        std::string names;
        for (const ServerPlayer *player: players) {
            if (!names.empty())
                names += ", ";
            names += player->getName();
        }

        return names;
    }
}

CameraShakeCommand::CameraShakeCommand(ServerNetworkHandler &handler)
        : Command("camerashake", "commands.screenshake.description",
                  "/camerashake add <player> [intensity] [seconds] [positional|rotational] | /camerashake stop [player]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> CameraShakeCommand::getOverloads() const {
    CommandOverloadData add;
    add.mParameters.push_back(makeEnumParameter("add", "CameraShakeAdd", {"add"}));
    add.mParameters.push_back(makePlayerParameter("player"));
    add.mParameters.push_back(makeTypedParameter("intensity", CommandParamType::Float, true));
    add.mParameters.push_back(makeTypedParameter("seconds", CommandParamType::Float, true));
    add.mParameters.push_back(makeEnumParameter("shakeType", "CameraShakeType", {"positional", "rotational"}, true));

    CommandOverloadData stop;
    stop.mParameters.push_back(makeEnumParameter("stop", "CameraShakeStop", {"stop"}));
    CommandParamData player = makePlayerParameter("player");
    player.mOptional = true;
    stop.mParameters.push_back(player);
    return {add, stop};
}

bool CameraShakeCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const bool adding = !arguments.empty() && arguments[0] == "add";
    const bool stopping = !arguments.empty() && arguments[0] == "stop";
    if ((!adding && !stopping) || (adding && arguments.size() < 2)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    std::vector<ServerPlayer *> targets;
    if (arguments.size() >= 2)
        targets = mHandler.resolveTargets(sender, arguments[1]);
    else if (sender.isPlayer())
        targets.push_back(sender.asPlayer());
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    CameraShakePacket packet;
    packet.mIntensity = DEFAULT_INTENSITY;
    packet.mDuration = DEFAULT_SECONDS;
    packet.mShakeType = CameraShakePacket::ShakeType::Positional;
    packet.mShakeAction = stopping ? CameraShakePacket::ShakeAction::Stop : CameraShakePacket::ShakeAction::Add;
    if (adding) {
        const bool valid = (arguments.size() < 3 || parseFloat(arguments[2], packet.mIntensity))
                           && (arguments.size() < 4 || parseFloat(arguments[3], packet.mDuration))
                           && (arguments.size() < 5 || arguments[4] == "positional" || arguments[4] == "rotational");
        if (!valid) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        if (arguments.size() >= 5 && arguments[4] == "rotational")
            packet.mShakeType = CameraShakePacket::ShakeType::Rotational;
    }

    for (ServerPlayer *target: targets)
        mHandler.sendPacketTo(target->getNetworkIdentifier(), packet);

    sender.sendTranslation(adding ? "commands.screenshake.success" : "commands.screenshake.successStop",
                           {joinNames(targets)});
    return true;
}
