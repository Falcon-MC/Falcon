#include "Command/KillCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

KillCommand::KillCommand(ServerNetworkHandler &handler)
        : Command("kill", "commands.kill.description", "/kill [player]"), mHandler(handler) {}

std::vector<CommandOverloadData> KillCommand::getOverloads() const {
    CommandParamData playerParameter = makePlayerParameter("player");
    playerParameter.mOptional = true;

    CommandOverloadData overload;
    overload.mParameters.push_back(playerParameter);

    return {overload};
}

bool KillCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    std::vector<ServerPlayer *> targets;

    if (arguments.empty()) {
        ServerPlayer *self = sender.asPlayer();
        if (self == nullptr) {
            sender.sendTranslation("commands.generic.targetNotPlayer", {});
            return false;
        }

        targets.push_back(self);
    } else {
        targets = mHandler.resolveTargets(sender, arguments[0]);
    }

    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    for (ServerPlayer *target: targets) {
        if (target->isDead())
            continue;

        mHandler.killPlayer(*target, "death.attack.generic", {target->getName()});
        sender.sendTranslation("commands.kill.successful", {target->getName()});
    }

    return true;
}
