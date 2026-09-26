#include "Command/DamageCommand.h"

#include "Actor/DamageCause.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <cstdlib>

DamageCommand::DamageCommand(ServerNetworkHandler &handler)
        : Command("damage", "commands.damage.description", "/damage <target> <amount> [cause]"), mHandler(handler) {}

std::vector<CommandOverloadData> DamageCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters = {makePlayerParameter("target"), makeTypedParameter("amount", CommandParamType::Int),
                            makeEnumParameter("cause", "DamageCause", DamageCause::getNames(), true)};
    return {overload};
}

bool DamageCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    char *end = nullptr;
    const float amount = std::strtof(arguments[1].c_str(), &end);
    if (end == arguments[1].c_str() || *end != '\0' || !std::isfinite(amount) || amount < 0.0f) {
        sender.sendTranslation("commands.damage.specify.damage", {});
        return false;
    }

    const char *deathMessageKey = "death.attack.generic";
    if (arguments.size() > 2) {
        deathMessageKey = DamageCause::findDeathMessageKey(arguments[2]);
        if (deathMessageKey == nullptr) {
            sender.sendTranslation("commands.generic.parameter.invalid", {arguments[2]});
            return false;
        }
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    for (ServerPlayer *target: targets) {
        const float healthBefore = target->getHealth();
        mHandler.hurtActor(*target, amount, deathMessageKey);

        if (target->getHealth() < healthBefore || target->isDead())
            sender.sendTranslation("commands.damage.success", {target->getName()});
        else
            sender.sendTranslation("commands.damage.failed", {target->getName()});
    }

    return true;
}
