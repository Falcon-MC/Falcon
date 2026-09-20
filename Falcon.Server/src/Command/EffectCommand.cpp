#include "Command/EffectCommand.h"

#include "Actor/MobEffect.h"
#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>

namespace {
    bool parseInteger(const std::string &value, int &out) {
        if (value.empty())
            return false;
        char *end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
            return false;
        out = (int) parsed;
        return true;
    }

    bool parseBoolean(const std::string &value, bool &out) {
        std::string lowered = value;
        for (char &c: lowered)
            c = (char) std::tolower((unsigned char) c);
        if (lowered == "true" || lowered == "1") {
            out = true;
            return true;
        }
        if (lowered == "false" || lowered == "0") {
            out = false;
            return true;
        }
        return false;
    }
}

EffectCommand::EffectCommand(ServerNetworkHandler &handler)
        : Command("effect", "commands.effect.description",
                  "/effect <player> <effect> [seconds] [amplifier] [hideParticles]"), mHandler(handler) {}

std::vector<CommandOverloadData> EffectCommand::getOverloads() const {
    CommandParamData effect;
    effect.mName = "effect";
    effect.mHasEnumData = true;
    effect.mEnumData.mName = "MobEffect";
    for (int id = 1; id <= 30; id++)
        effect.mEnumData.mValues.push_back(getMobEffectName((MobEffectId) id));
    effect.mEnumData.mValues.push_back("clear");

    CommandParamData seconds;
    seconds.mName = "seconds";
    seconds.mOptional = true;
    seconds.mHasType = true;
    seconds.mType = CommandParamType::Int;

    CommandParamData amplifier;
    amplifier.mName = "amplifier";
    amplifier.mOptional = true;
    amplifier.mHasType = true;
    amplifier.mType = CommandParamType::Int;

    CommandParamData hideParticles;
    hideParticles.mName = "hideParticles";
    hideParticles.mOptional = true;
    hideParticles.mHasType = true;
    hideParticles.mType = CommandParamType::String;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("player"));
    overload.mParameters.push_back(effect);
    overload.mParameters.push_back(seconds);
    overload.mParameters.push_back(amplifier);
    overload.mParameters.push_back(hideParticles);
    return {overload};
}

bool EffectCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.player.notFound", {});
        return false;
    }

    if (arguments[1] == "clear") {
        MobEffectId only;
        const bool clearOne = arguments.size() > 2 && parseMobEffect(arguments[2], only);
        if (arguments.size() > 2 && !clearOne) {
            sender.sendTranslation("commands.effect.notFound", {arguments[2]});
            return false;
        }

        int removed = 0;
        for (ServerPlayer *target: targets) {
            if (clearOne) {
                removed += target->removeEffect(only) ? 1 : 0;
            } else {
                for (int id = 1; id <= 30; id++)
                    removed += target->removeEffect((MobEffectId) id) ? 1 : 0;
            }
        }
        if (removed == 0) {
            sender.sendTranslation(clearOne ? "commands.effect.failure.notActive" :
                                               "commands.effect.failure.notActive.all",
                                   clearOne ? std::vector<std::string>{getMobEffectName(only), targets.front()->getName()} :
                                              std::vector<std::string>{targets.front()->getName()});
        } else {
            sender.sendTranslation(clearOne ? "commands.effect.success.removed" :
                                               "commands.effect.success.removed.all",
                                   clearOne ? std::vector<std::string>{getMobEffectName(only), targets.front()->getName()} :
                                              std::vector<std::string>{targets.front()->getName()});
        }
        return removed != 0;
    }

    MobEffectId id;
    if (!parseMobEffect(arguments[1], id)) {
        sender.sendTranslation("commands.effect.notFound", {arguments[1]});
        return false;
    }

    int seconds = 30;
    int amplifier = 0;
    bool particles = true;
    bool infinite = false;
    if (arguments.size() > 2) {
        if (arguments[2] == "infinite") {
            infinite = true;
        } else if (!parseInteger(arguments[2], seconds) || seconds < 1) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
    }
    if (arguments.size() > 3 && (!parseInteger(arguments[3], amplifier) || amplifier < 0 || amplifier > 255)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    if (arguments.size() > 4 && !parseBoolean(arguments[4], particles)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    MobEffectInstance instance;
    instance.mId = id;
    const long long durationTicks = (long long) seconds * 20LL;
    instance.mDuration = infinite ? -1 : (int32_t) std::min(durationTicks, (long long) INT_MAX);
    instance.mInfinite = infinite;
    instance.mAmplifier = amplifier;
    instance.mParticles = particles;

    int applied = 0;
    for (ServerPlayer *target: targets)
        applied += target->addEffect(instance) ? 1 : 0;

    if (applied == 0) {
        sender.sendTranslation("commands.effect.failure.notActive", {getMobEffectName(id), targets.front()->getName()});
    } else if (infinite) {
        sender.sendTranslation("commands.effect.success.infinite",
                               {getMobEffectName(id), std::to_string(amplifier), targets.front()->getName()});
    } else {
        sender.sendTranslation("commands.effect.success",
                               {getMobEffectName(id), std::to_string(amplifier), targets.front()->getName(),
                                std::to_string(seconds)});
    }
    return applied != 0;
}
