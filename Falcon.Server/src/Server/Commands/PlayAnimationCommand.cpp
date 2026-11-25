#include "Server/Commands/PlayAnimationCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/AnimateEntityPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"

namespace {
    const char *const DEFAULT_NEXT_STATE = "default";
    const char *const DEFAULT_STOP_EXPRESSION = "query.any_animation_finished";
    const char *const DEFAULT_CONTROLLER = "__runtime_controller";
    // the molang version the game's command gives the stop expression
    const int32_t STOP_EXPRESSION_VERSION = 1;
}

PlayAnimationCommand::PlayAnimationCommand(ServerNetworkHandler &handler)
        : Command("playanimation", "commands.playanimation.description",
                  "/playanimation <entity: target> <animation: string> [next_state: string] [blend_out_time: float] "
                  "[stop_expression: string] [controller: string]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> PlayAnimationCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makeTypedParameter("entity", CommandParamType::Target));
    overload.mParameters.push_back(makeTypedParameter("animation", CommandParamType::String));
    overload.mParameters.push_back(makeTypedParameter("next_state", CommandParamType::String, true));
    overload.mParameters.push_back(makeTypedParameter("blend_out_time", CommandParamType::Float, true));
    overload.mParameters.push_back(makeTypedParameter("stop_expression", CommandParamType::String, true));
    overload.mParameters.push_back(makeTypedParameter("controller", CommandParamType::String, true));
    return {overload};
}

bool PlayAnimationCommand::readAnimation(const std::vector<std::string> &arguments, AnimateEntityPacket &packet,
                                         std::string &invalid) {
    packet.mAnimation = arguments.empty() ? std::string() : arguments[0];
    packet.mNextState = arguments.size() > 1 ? arguments[1] : DEFAULT_NEXT_STATE;
    packet.mBlendOutTime = 0.0f;
    if (arguments.size() > 2
        && (!parseCoordinate(arguments[2], 0.0f, packet.mBlendOutTime) || arguments[2][0] == '~')) {
        invalid = arguments[2];
        return false;
    }
    packet.mStopExpression = arguments.size() > 3 ? arguments[3] : DEFAULT_STOP_EXPRESSION;
    packet.mStopExpressionVersion = STOP_EXPRESSION_VERSION;
    packet.mController = arguments.size() > 4 ? arguments[4] : DEFAULT_CONTROLLER;
    return true;
}

bool PlayAnimationCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sendUsageError(sender);
        return false;
    }

    AnimateEntityPacket packet;
    std::string invalid;
    if (!readAnimation(std::vector<std::string>(arguments.begin() + 1, arguments.end()), packet, invalid)) {
        sender.sendTranslation("commands.generic.num.invalid", {invalid});
        return false;
    }

    // any entity the selector names, players first, all in one packet
    for (ServerPlayer *player: mHandler.resolveTargets(sender, arguments[0]))
        packet.mRuntimeActorIds.push_back(player->getRuntimeId());
    for (ServerActor *actor: mHandler.resolveActorTargets(sender, arguments[0]))
        packet.mRuntimeActorIds.push_back(actor->getRuntimeId());
    if (packet.mRuntimeActorIds.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    mHandler.broadcastToSpawned(packet);
    sender.sendTranslation("commands.playanimation.success", {});
    return true;
}
