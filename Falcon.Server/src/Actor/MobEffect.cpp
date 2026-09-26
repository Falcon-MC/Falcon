#include "Actor/MobEffect.h"

#include "Actor/Actor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <vector>

namespace {
    constexpr const char *HEALTH = "minecraft:health";
    constexpr const char *MOVEMENT = "minecraft:movement";
    constexpr const char *ABSORPTION = "minecraft:absorption";

    std::string lower(std::string value) {
        for (char &c: value)
            c = (char) std::tolower((unsigned char) c);
        return value;
    }
}

const char *getMobEffectName(MobEffectId id) {
    switch (id) {
        case MobEffectId::Speed: return "speed";
        case MobEffectId::Slowness: return "slowness";
        case MobEffectId::Haste: return "haste";
        case MobEffectId::MiningFatigue: return "mining_fatigue";
        case MobEffectId::Strength: return "strength";
        case MobEffectId::InstantHealth: return "instant_health";
        case MobEffectId::InstantDamage: return "instant_damage";
        case MobEffectId::JumpBoost: return "jump_boost";
        case MobEffectId::Nausea: return "nausea";
        case MobEffectId::Regeneration: return "regeneration";
        case MobEffectId::Resistance: return "resistance";
        case MobEffectId::FireResistance: return "fire_resistance";
        case MobEffectId::WaterBreathing: return "water_breathing";
        case MobEffectId::Invisibility: return "invisibility";
        case MobEffectId::Blindness: return "blindness";
        case MobEffectId::NightVision: return "night_vision";
        case MobEffectId::Hunger: return "hunger";
        case MobEffectId::Weakness: return "weakness";
        case MobEffectId::Poison: return "poison";
        case MobEffectId::Wither: return "wither";
        case MobEffectId::HealthBoost: return "health_boost";
        case MobEffectId::Absorption: return "absorption";
        case MobEffectId::Saturation: return "saturation";
        case MobEffectId::Levitation: return "levitation";
        case MobEffectId::FatalPoison: return "fatal_poison";
        case MobEffectId::ConduitPower: return "conduit_power";
        case MobEffectId::SlowFalling: return "slow_falling";
        case MobEffectId::BadOmen: return "bad_omen";
        case MobEffectId::VillageHero: return "village_hero";
        case MobEffectId::Darkness: return "darkness";
    }
    return "unknown";
}

bool parseMobEffect(const std::string &value, MobEffectId &id) {
    std::string key = lower(value);
    if (key.rfind("minecraft:", 0) == 0)
        key.erase(0, 10);
    for (int32_t raw = 1; raw <= 30; raw++) {
        MobEffectId candidate = (MobEffectId) raw;
        if (key == getMobEffectName(candidate)) {
            id = candidate;
            return true;
        }
    }
    try {
        size_t consumed = 0;
        int raw = std::stoi(key, &consumed);
        if (consumed == key.size() && raw >= 1 && raw <= 30) {
            id = (MobEffectId) raw;
            return true;
        }
    } catch (...) {}
    return false;
}

bool parseDefinitionMobEffect(const std::string &value, MobEffectId &id) {
    const std::string key = lower(value);
    if (key == "heal") {
        id = MobEffectId::InstantHealth;
        return true;
    }
    if (key == "harm") {
        id = MobEffectId::InstantDamage;
        return true;
    }
    return parseMobEffect(key, id);
}

void AbsorptionMobEffect::onAdd(Actor &actor, const MobEffectInstance &instance) const {
    actor.getAttributes().setClamped(ABSORPTION, 4.0f * (float) instance.level());
}

void AbsorptionMobEffect::onRemove(Actor &actor, const MobEffectInstance &) const {
    actor.getAttributes().setClamped(ABSORPTION, 0.0f);
}

void HealthBoostMobEffect::onAdd(Actor &actor, const MobEffectInstance &instance) const {
    const float amount = 4.0f * (float) instance.level();
    actor.getAttributes().addMaximum(HEALTH, amount);
    actor.getAttributes().setClamped(HEALTH, actor.getHealth() + amount);
}

void HealthBoostMobEffect::onRemove(Actor &actor, const MobEffectInstance &instance) const {
    actor.getAttributes().addMaximum(HEALTH, -4.0f * (float) instance.level());
    actor.getAttributes().setClamped(HEALTH, actor.getHealth());
}

void HungerMobEffect::tick(Actor &actor, const MobEffectInstance &instance) const {
    actor.exhaust(0.1f * (float) instance.level());
}

void InstantHealthMobEffect::tick(Actor &actor, const MobEffectInstance &instance) const {
    actor.heal((float) (4 << std::min(instance.mAmplifier, 7)));
}

void InstantDamageMobEffect::tick(Actor &actor, const MobEffectInstance &instance) const {
    actor.reduceHealth((float) (6 << std::min(instance.mAmplifier, 7)));
}

void InvisibilityMobEffect::onAdd(Actor &actor, const MobEffectInstance &) const {
    actor.getFlags().set(ActorFlag::Invisible, true);
}

void InvisibilityMobEffect::onRemove(Actor &actor, const MobEffectInstance &) const {
    actor.getFlags().set(ActorFlag::Invisible, false);
}

void LevitationMobEffect::onAdd(Actor &actor, const MobEffectInstance &) const {
    actor.getFlags().set(ActorFlag::HasGravity, false);
}

void LevitationMobEffect::onRemove(Actor &actor, const MobEffectInstance &) const {
    actor.getFlags().set(ActorFlag::HasGravity, true);
}

void LevitationMobEffect::tick(Actor &actor, const MobEffectInstance &instance) const {
    Vector3f motion = actor.getMotion();
    const float target = std::min(0.05f * (float) instance.level(), 0.5f);
    motion.y += (target - motion.y) * 0.5f;
    actor.setMotion(motion);
}

int32_t PoisonMobEffect::getApplyInterval(const MobEffectInstance &instance) const {
    return std::max(1, 25 >> std::min(instance.mAmplifier, 4));
}

void PoisonMobEffect::tick(Actor &actor, const MobEffectInstance &) const {
    if (mFatal || actor.getHealth() > 1.0f)
        actor.reduceHealth(1.0f);
}

int32_t RegenerationMobEffect::getApplyInterval(const MobEffectInstance &instance) const {
    return std::max(1, 50 >> std::min(instance.mAmplifier, 5));
}

void RegenerationMobEffect::tick(Actor &actor, const MobEffectInstance &) const {
    if (actor.getHealth() < actor.getMaxHealth())
        actor.heal(1.0f);
}

void SaturationMobEffect::tick(Actor &actor, const MobEffectInstance &instance) const {
    actor.addFood((float) instance.level());
    actor.addSaturation((float) instance.level() * 2.0f);
}

int32_t WitherMobEffect::getApplyInterval(const MobEffectInstance &instance) const {
    return std::max(1, 40 >> std::min(instance.mAmplifier, 5));
}

void WitherMobEffect::tick(Actor &actor, const MobEffectInstance &) const {
    actor.reduceHealth(1.0f);
}

std::unique_ptr<MobEffect> MobEffectManager::create(MobEffectId id) {
    switch (id) {
        case MobEffectId::Absorption: return std::make_unique<AbsorptionMobEffect>();
        case MobEffectId::HealthBoost: return std::make_unique<HealthBoostMobEffect>();
        case MobEffectId::Hunger: return std::make_unique<HungerMobEffect>();
        case MobEffectId::InstantHealth: return std::make_unique<InstantHealthMobEffect>();
        case MobEffectId::InstantDamage: return std::make_unique<InstantDamageMobEffect>();
        case MobEffectId::Invisibility: return std::make_unique<InvisibilityMobEffect>();
        case MobEffectId::Levitation: return std::make_unique<LevitationMobEffect>();
        case MobEffectId::Poison: return std::make_unique<PoisonMobEffect>();
        case MobEffectId::FatalPoison: return std::make_unique<PoisonMobEffect>(true);
        case MobEffectId::Regeneration: return std::make_unique<RegenerationMobEffect>();
        case MobEffectId::Saturation: return std::make_unique<SaturationMobEffect>();
        case MobEffectId::Slowness: return std::make_unique<SlownessMobEffect>();
        case MobEffectId::Speed: return std::make_unique<SpeedMobEffect>();
        case MobEffectId::Wither: return std::make_unique<WitherMobEffect>();
        case MobEffectId::Strength: return std::make_unique<AttackDamageMobEffect>();
        case MobEffectId::BadOmen: return std::make_unique<RaidOmenMobEffect>();
        default: return std::make_unique<MobEffect>();
    }
}

const MobEffect &MobEffectManager::effect(MobEffectId id) const {
    static std::unordered_map<int32_t, std::unique_ptr<MobEffect>> effects;
    const int32_t key = (int32_t) id;
    auto it = effects.find(key);
    if (it == effects.end())
        it = effects.emplace(key, create(id)).first;
    return *it->second;
}

void MobEffectManager::emit(const MobEffectInstance &instance, MobEffectEvent eventType) const {
    if (mEventCallback)
        mEventCallback(instance, eventType);
}

bool MobEffectManager::add(const MobEffectInstance &input) {
    if (input.mDuration == 0 && !input.mInfinite)
        return false;

    if (sAddFilter && !sAddFilter(mActor, input))
        return false;

    MobEffectInstance instance = input;
    instance.mAmplifier = std::clamp(instance.mAmplifier, 0, 255);
    const int32_t key = (int32_t) instance.mId;
    auto existing = mEffects.find(key);
    if (existing != mEffects.end()) {
        if (instance.mAmplifier < existing->second.mAmplifier
            || (instance.mAmplifier == existing->second.mAmplifier
                && !instance.mInfinite && instance.mDuration <= existing->second.mDuration))
            return false;
        effect(existing->second.mId).onRemove(mActor, existing->second);
        existing->second = instance;
        effect(instance.mId).onAdd(mActor, instance);
        refreshDerivedAttributes();
        emit(instance, MobEffectEvent::Modify);
        return true;
    }

    mEffects.emplace(key, instance);
    effect(instance.mId).onAdd(mActor, instance);
    refreshDerivedAttributes();
    emit(instance, MobEffectEvent::Add);
    return true;
}

bool MobEffectManager::remove(MobEffectId id) {
    auto it = mEffects.find((int32_t) id);
    if (it == mEffects.end())
        return false;
    MobEffectInstance instance = it->second;
    effect(id).onRemove(mActor, instance);
    mEffects.erase(it);
    refreshDerivedAttributes();
    emit(instance, MobEffectEvent::Remove);
    return true;
}

void MobEffectManager::clear() {
    while (!mEffects.empty())
        remove((MobEffectId) mEffects.begin()->first);
}

bool MobEffectManager::tick(int32_t tickDiff) {
    if (tickDiff <= 0 || mEffects.empty() || !mActor.isAlive())
        return false;
    const float oldHealth = mActor.getHealth();
    const float oldFood = mActor.getFood();
    const float oldSaturation = mActor.getSaturation();
    const float oldExhaustion = mActor.getExhaustion();
    const float oldMaxHealth = mActor.getMaxHealth();
    const float oldMovement = mActor.getAttributes().get("minecraft:movement");
    const int64_t oldFlags = mActor.getFlags().getLowBits();
    const int64_t oldFlags2 = mActor.getFlags().getHighBits();
    std::vector<MobEffectId> expired;
    for (auto &entry: mEffects) {
        MobEffectInstance &instance = entry.second;
        const MobEffect &type = effect(instance.mId);
        const int32_t interval = std::max(1, type.getApplyInterval(instance));
        const int32_t elapsed = instance.mInfinite ? 0 : instance.mDuration;
        if (type.isInstantaneous()) {
            type.tick(mActor, instance);
            expired.push_back(instance.mId);
            continue;
        }
        if (elapsed % interval == 0)
            type.tick(mActor, instance);
        if (!instance.mInfinite) {
            instance.mDuration -= tickDiff;
            if (instance.mDuration <= 0)
                expired.push_back(instance.mId);
        }
    }
    for (MobEffectId id: expired)
        remove(id);
    return oldHealth != mActor.getHealth()
           || oldFood != mActor.getFood()
           || oldSaturation != mActor.getSaturation()
           || oldExhaustion != mActor.getExhaustion()
           || oldMaxHealth != mActor.getMaxHealth()
           || oldMovement != mActor.getAttributes().get("minecraft:movement")
           || oldFlags != mActor.getFlags().getLowBits()
           || oldFlags2 != mActor.getFlags().getHighBits();
}

bool MobEffectManager::has(MobEffectId id) const {
    return mEffects.find((int32_t) id) != mEffects.end();
}

const MobEffectInstance *MobEffectManager::get(MobEffectId id) const {
    auto it = mEffects.find((int32_t) id);
    return it == mEffects.end() ? nullptr : &it->second;
}

float MobEffectManager::movementSpeedMultiplier() const {
    float multiplier = 1.0f;
    if (const MobEffectInstance *speed = get(MobEffectId::Speed))
        multiplier *= 1.0f + 0.2f * (float) speed->level();
    if (const MobEffectInstance *slowness = get(MobEffectId::Slowness))
        multiplier *= std::max(0.0f, 1.0f - 0.15f * (float) slowness->level());
    return multiplier;
}

float MobEffectManager::jumpVelocityMultiplier() const {
    if (const MobEffectInstance *jump = get(MobEffectId::JumpBoost))
        return 1.0f + 0.1f * (float) jump->level();
    return 1.0f;
}

void MobEffectManager::refreshDerivedAttributes() {
    mActor.getAttributes().set(MOVEMENT, 0.1f * movementSpeedMultiplier());
    if (has(MobEffectId::Invisibility))
        mActor.getFlags().set(ActorFlag::Invisible, true);
    else
        mActor.getFlags().set(ActorFlag::Invisible, false);
    if (!has(MobEffectId::Levitation))
        mActor.getFlags().set(ActorFlag::HasGravity, true);
    mAttributesDirty = true;
}
