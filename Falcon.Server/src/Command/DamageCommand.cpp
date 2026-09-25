#include "Command/DamageCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <cstdlib>

namespace {
    struct DamageCause {
        const char *mName;
        const char *mDeathMessageKey;
    };

    const DamageCause DAMAGE_CAUSES[] = {
            {"anvil", "death.attack.anvil"},
            {"drowning", "death.attack.drown"},
            {"entity_explosion", "death.attack.explosion"},
            {"block_explosion", "death.attack.explosion"},
            {"fall", "death.fell.accident.generic"},
            {"fire", "death.attack.inFire"},
            {"fire_tick", "death.attack.onFire"},
            {"freezing", "death.attack.freeze"},
            {"lava", "death.attack.lava"},
            {"lightning", "death.attack.lightningBolt"},
            {"magic", "death.attack.magic"},
            {"magma", "death.attack.hotFloor"},
            {"projectile", "death.attack.arrow"},
            {"stalagmite", "death.attack.stalagmite"},
            {"starve", "death.attack.starve"},
            {"suffocation", "death.attack.inWall"},
            {"thorns", "death.attack.thorns"},
            {"void", "death.attack.outOfWorld"},
            {"override", "death.attack.generic"}
    };

    const char *deathMessageKeyFor(const std::string &cause) {
        for (const DamageCause &entry: DAMAGE_CAUSES) {
            if (cause == entry.mName)
                return entry.mDeathMessageKey;
        }

        return nullptr;
    }

    std::vector<std::string> causeNames() {
        std::vector<std::string> names;
        for (const DamageCause &entry: DAMAGE_CAUSES)
            names.push_back(entry.mName);
        return names;
    }
}

const char *DamageCommand::findDeathMessageKey(const std::string &cause) {
    return deathMessageKeyFor(cause);
}

DamageCommand::DamageCommand(ServerNetworkHandler &handler)
        : Command("damage", "commands.damage.description", "/damage <target> <amount> [cause]"), mHandler(handler) {}

std::vector<CommandOverloadData> DamageCommand::getOverloads() const {
    CommandOverloadData overload;
    overload.mParameters = {makePlayerParameter("target"), makeTypedParameter("amount", CommandParamType::Int),
                            makeEnumParameter("cause", "DamageCause", causeNames(), true)};
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
        deathMessageKey = deathMessageKeyFor(arguments[2]);
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
