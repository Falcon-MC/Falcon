#include "Actor/Mob/Component/MobAnger.h"

#include "Actor/Definition/EntityEvents.h"
#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {
    const char *const ANGRY_COMPONENT = "minecraft:angry";
    const int32_t TICKS_PER_SECOND = 20;
    const float DEFAULT_DURATION = 25.0f;
    const float DEFAULT_BROADCAST_RANGE = 20.0f;
    const float DEFAULT_SOUND_MIN = 0.0f;
    const float DEFAULT_SOUND_MAX = 0.0f;

    std::mt19937 &angerRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float numberIn(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    bool flagIn(const json::Value &component, const char *key) {
        const json::Value *value = component.get(key);
        return value != nullptr && value->boolean(false);
    }

    std::vector<std::string> broadcastTargets(const json::Value &component) {
        std::vector<std::string> targets;
        const json::Value *list = component.get("broadcast_targets");
        if (list == nullptr || !list->isArray())
            return targets;

        for (const std::unique_ptr<json::Value> &entry: list->mArray)
            targets.push_back(entry->string());
        return targets;
    }

    bool hasAnyFamily(const MobActor &mob, const std::vector<std::string> &families) {
        if (families.empty())
            return true;

        for (const std::string &family: mob.getFamilies()) {
            if (std::find(families.begin(), families.end(), family) != families.end())
                return true;
        }
        return false;
    }

    void setAngryFlag(ServerNetworkHandler &owner, MobActor &mob, bool angry) {
        if (mob.getFlags().get(ActorFlag::Angry) == angry)
            return;

        mob.getFlags().set(ActorFlag::Angry, angry);
        owner.syncActorFlags(mob);
    }
}

void MobAnger::tick(ServerNetworkHandler &owner, MobActor &mob) {
    const json::Value *component = mob.getComponent(ANGRY_COMPONENT);
    if (component != mComponent) {
        mComponent = component;
        if (component == nullptr) {
            setAngryFlag(owner, mob, false);
            return;
        }
        _start(owner, mob, *component);
        if (mComponent == nullptr)
            return;
    }

    if (component == nullptr)
        return;

    const json::Value *sound = component->get("angry_sound");
    if (sound != nullptr && --mSoundTicks <= 0) {
        mob.playDefinitionSound(owner, sound->string());
        _resetSound(*component);
    }

    if (mTicks > 0 && --mTicks == 0)
        _calm(owner, mob, *component);
}

void MobAnger::onHurt(ServerNetworkHandler &owner, MobActor &mob, Actor *attacker) {
    if (mComponent == nullptr || attacker == nullptr)
        return;

    _resetDuration(*mComponent);
    if (flagIn(*mComponent, "broadcast_anger_on_being_attacked"))
        _broadcast(owner, mob, *mComponent, *attacker);
}

void MobAnger::onAttack(ServerNetworkHandler &owner, MobActor &mob, Actor &victim) {
    if (mComponent != nullptr && flagIn(*mComponent, "broadcast_anger_on_attack"))
        _broadcast(owner, mob, *mComponent, victim);
}

void MobAnger::_start(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component) {
    Actor *target = mob.getTarget(owner);
    const json::Value *filters = component.get("filters");
    if (target != nullptr && filters != nullptr && !EntityFilter::test(*filters, owner, mob, target)) {
        _calm(owner, mob, component);
        return;
    }

    setAngryFlag(owner, mob, true);
    _resetDuration(component);
    _resetSound(component);

    if (target != nullptr && flagIn(component, "broadcast_anger"))
        _broadcast(owner, mob, component, *target);
}

void MobAnger::_calm(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component) {
    mComponent = nullptr;
    setAngryFlag(owner, mob, false);
    mob.clearTarget();
    EntityEvents::fireTrigger(owner, mob, component.get("calm_event"));
}

void MobAnger::_broadcast(ServerNetworkHandler &owner, MobActor &mob, const json::Value &component,
                          const Actor &target) {
    const float range = numberIn(component, "broadcast_range", DEFAULT_BROADCAST_RANGE);
    const std::vector<std::string> families = broadcastTargets(component);

    for (auto &entry: owner.getActors()) {
        MobActor *ally = dynamic_cast<MobActor *>(entry.second.get());
        if (ally == nullptr || ally == &mob || !ally->isAlive() || ally->getDimension() != mob.getDimension()
            || ally->getTarget(owner) != nullptr || !hasAnyFamily(*ally, families)
            || mob.distanceSquaredTo(*ally) > range * range || !ally->canTarget(target))
            continue;

        ally->setTarget(owner, target.getRuntimeId());
    }
}

void MobAnger::_resetDuration(const json::Value &component) {
    const float seconds = numberIn(component, "duration", DEFAULT_DURATION);
    mTicks = seconds < 0.0f ? -1 : std::max(1, (int32_t) std::lround(seconds * (float) TICKS_PER_SECOND));
}

void MobAnger::_resetSound(const json::Value &component) {
    const json::Value *interval = component.get("sound_interval");
    float minimum = DEFAULT_SOUND_MIN;
    float maximum = DEFAULT_SOUND_MAX;
    if (interval != nullptr && interval->isObject()) {
        minimum = numberIn(*interval, "range_min", numberIn(*interval, "min", minimum));
        maximum = numberIn(*interval, "range_max", numberIn(*interval, "max", minimum));
    }

    const float seconds = maximum > minimum ? std::uniform_real_distribution<float>(minimum, maximum)(angerRandom())
                                            : minimum;
    mSoundTicks = std::max(1, (int32_t) std::lround(seconds * (float) TICKS_PER_SECOND));
}
