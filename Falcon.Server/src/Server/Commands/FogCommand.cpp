#include "Server/Commands/FogCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/FogStack.h"

FogCommand::FogCommand(ServerNetworkHandler &handler)
        : Command("fog", "commands.fog.description",
                  "/fog <victim> push <fogId> <userProvidedId> | /fog <victim> <pop|remove> <userProvidedId>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> FogCommand::getOverloads() const {
    CommandOverloadData push;
    push.mParameters.push_back(makePlayerParameter("victim"));
    push.mParameters.push_back(makeEnumParameter("mode", "FogCommandPushMode", {"push"}));
    push.mParameters.push_back(makeTypedParameter("fogId", CommandParamType::String));
    push.mParameters.push_back(makeTypedParameter("userProvidedId", CommandParamType::String));

    CommandOverloadData remove;
    remove.mParameters.push_back(makePlayerParameter("victim"));
    remove.mParameters.push_back(makeEnumParameter("mode", "FogCommandDeleteMode", {"pop", "remove"}));
    remove.mParameters.push_back(makeTypedParameter("userProvidedId", CommandParamType::String));
    return {push, remove};
}

bool FogCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const bool pushing = arguments.size() == 4 && arguments[1] == "push";
    const bool popping = arguments.size() == 3 && arguments[1] == "pop";
    const bool removing = arguments.size() == 3 && arguments[1] == "remove";
    if (!pushing && !popping && !removing) {
        sendUsageError(sender);
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }
    if (pushing && !FogStack::isValidId(arguments[2])) {
        sender.sendTranslation("commands.fog.invalidFogId", {arguments[2]});
        return false;
    }

    const std::string &userId = arguments.back();
    bool changed = false;
    for (ServerPlayer *target: targets) {
        FogStack::Entries &stack = target->getFogStack();
        bool done = false;
        if (pushing)
            done = FogStack::push(stack, arguments[2], userId.empty() ? std::string(FogStack::UNTAGGED) : userId);
        else if (popping)
            done = !FogStack::pop(stack, &userId).empty();
        else
            done = FogStack::remove(stack, &userId);
        if (!done) {
            sender.sendTranslation(pushing ? "commands.fog.limitReached" : "commands.fog.invalidUserId",
                                   {target->getName(), userId});
            continue;
        }
        FogStack::send(mHandler, *target);
        sender.sendTranslation(pushing   ? "commands.fog.success.push"
                               : popping ? "commands.fog.success.pop"
                                         : "commands.fog.success.remove",
                               {target->getName(), userId});
        changed = true;
    }
    return changed;
}
