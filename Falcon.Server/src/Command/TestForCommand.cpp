#include "Command/TestForCommand.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

TestForCommand::TestForCommand(ServerNetworkHandler &handler)
        : Command("testfor", "commands.testfor.description", "/testfor <victim>"), mHandler(handler) {}

std::vector<CommandOverloadData> TestForCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("victim"));
    return {overload};
}

bool TestForCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty()) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    const std::vector<ServerActor *> actors = mHandler.resolveActorTargets(sender, arguments[0]);
    if (targets.empty() && actors.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    std::string names;
    for (const ServerPlayer *target: targets) {
        if (!names.empty())
            names += ", ";
        names += target->getName();
    }

    for (const ServerActor *actor: actors) {
        if (!names.empty())
            names += ", ";
        names += actor->getName();
    }

    sender.sendTranslation("commands.testfor.success", {names});
    return true;
}
