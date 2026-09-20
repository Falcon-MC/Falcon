#include "Command/XpCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cerrno>
#include <cstdlib>
#include <string>

XpCommand::XpCommand(ServerNetworkHandler &handler)
        : Command("xp", "commands.xp.description", "/xp <amount>[L] [player]"), mHandler(handler) {}

std::vector<CommandOverloadData> XpCommand::getOverloads() const {
    CommandParamData amount;
    amount.mName = "amount";
    amount.mHasType = true;
    amount.mType = CommandParamType::String;

    CommandParamData player = makePlayerParameter("player", mHandler.getPlayerNames());
    player.mOptional = true;

    CommandOverloadData overload;
    overload.mParameters.push_back(amount);
    overload.mParameters.push_back(player);

    return {overload};
}

bool XpCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.empty() || arguments.size() > 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    std::string value = arguments[0];
    const bool levels = !value.empty() && (value.back() == 'L' || value.back() == 'l');
    if (levels)
        value.pop_back();

    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (value.empty() || end == value.c_str() || *end != '\0' || errno == ERANGE) {
        sender.sendTranslation("commands.generic.num.invalid", {arguments[0]});
        return false;
    }

    const int32_t amount = (int32_t) parsed;

    if (!levels && amount < 0) {
        sender.sendTranslation("commands.xp.failure.widthdrawXp", {});
        return false;
    }

    std::vector<ServerPlayer *> targets;
    if (arguments.size() == 2) {
        targets = mHandler.resolveTargets(sender, arguments[1]);
    } else {
        ServerPlayer *self = sender.asPlayer();
        if (self == nullptr) {
            sender.sendTranslation("commands.generic.targetNotPlayer", {});
            return false;
        }

        targets.push_back(self);
    }

    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    for (ServerPlayer *target: targets) {
        if (levels)
            target->getExperience().addXpLevels(amount);
        else
            target->getExperience().addXp(amount);

        target->syncExperience();
        mHandler.syncPlayerAttributes(*target);

        if (!levels)
            sender.sendTranslation("commands.xp.success", {std::to_string(amount), target->getName()});
        else if (amount < 0)
            sender.sendTranslation("commands.xp.success.negative.levels",
                                   {std::to_string(-amount), target->getName()});
        else
            sender.sendTranslation("commands.xp.success.levels", {std::to_string(amount), target->getName()});
    }

    return true;
}
