#include "Command/StopSoundCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/StopSoundPacket.h"

StopSoundCommand::StopSoundCommand(ServerNetworkHandler &handler)
        : Command("stopsound", "commands.stopsound.description", "/stopsound <player> [sound]"), mHandler(handler) {}

std::vector<CommandOverloadData> StopSoundCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters = {makePlayerParameter("player"), makeTypedParameter("sound", CommandParamType::String, true)};
    return {overload};
}

bool StopSoundCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    StopSoundPacket packet;
    packet.mSoundName = arguments.size() > 1 ? arguments[1] : std::string();
    packet.mStoppingAllSound = arguments.size() < 2;
    packet.mStopMusicLegacy = false;

    for (ServerPlayer *target: targets)
        mHandler.sendPacketTo(target->getNetworkIdentifier(), packet);

    if (packet.mStoppingAllSound)
        sender.sendTranslation("commands.stopsound.success.all", {});
    else
        sender.sendTranslation("commands.stopsound.success", {packet.mSoundName});
    return true;
}
