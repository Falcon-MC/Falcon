#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

class Actor;

enum class MobEffectId : int32_t {
    Speed = 1,
    Slowness = 2,
    Haste = 3,
    MiningFatigue = 4,
    Strength = 5,
    InstantHealth = 6,
    InstantDamage = 7,
    JumpBoost = 8,
    Nausea = 9,
    Regeneration = 10,
    Resistance = 11,
    FireResistance = 12,
    WaterBreathing = 13,
    Invisibility = 14,
    Blindness = 15,
    NightVision = 16,
    Hunger = 17,
    Weakness = 18,
    Poison = 19,
    Wither = 20,
    HealthBoost = 21,
    Absorption = 22,
    Saturation = 23,
    Levitation = 24,
    FatalPoison = 25,
    ConduitPower = 26,
    SlowFalling = 27,
    BadOmen = 28,
    VillageHero = 29,
    Darkness = 30
};

enum class MobEffectEvent : uint8_t { Add, Modify, Remove };

struct MobEffectInstance {
    MobEffectId mId = MobEffectId::Speed;
    int32_t mDuration = 0;
    int32_t mAmplifier = 0;
    bool mParticles = true;
    bool mAmbient = false;
    bool mInfinite = false;

    int32_t level() const { return mAmplifier + 1; }
};

const char *getMobEffectName(MobEffectId id);
bool parseMobEffect(const std::string &value, MobEffectId &id);
bool parseDefinitionMobEffect(const std::string &value, MobEffectId &id);

class MobEffect {
public:
    virtual ~MobEffect() = default;

    virtual void onAdd(Actor &, const MobEffectInstance &) const {}
    virtual void onRemove(Actor &, const MobEffectInstance &) const {}
    virtual void tick(Actor &, const MobEffectInstance &) const {}
    virtual int32_t getApplyInterval(const MobEffectInstance &) const { return 1; }
    virtual bool isInstantaneous() const { return false; }
};

class AbsorptionMobEffect final : public MobEffect {
public:
    void onAdd(Actor &, const MobEffectInstance &) const override;
    void onRemove(Actor &, const MobEffectInstance &) const override;
};

class HealthBoostMobEffect final : public MobEffect {
public:
    void onAdd(Actor &, const MobEffectInstance &) const override;
    void onRemove(Actor &, const MobEffectInstance &) const override;
};

class HungerMobEffect final : public MobEffect {
public:
    void tick(Actor &, const MobEffectInstance &) const override;
};

class InstantaneousMobEffect : public MobEffect {
public:
    bool isInstantaneous() const override { return true; }
};

class InstantHealthMobEffect final : public InstantaneousMobEffect {
public:
    void tick(Actor &, const MobEffectInstance &) const override;
};

class InstantDamageMobEffect final : public InstantaneousMobEffect {
public:
    void tick(Actor &, const MobEffectInstance &) const override;
};

class InvisibilityMobEffect final : public MobEffect {
public:
    void onAdd(Actor &, const MobEffectInstance &) const override;
    void onRemove(Actor &, const MobEffectInstance &) const override;
};

class LevitationMobEffect final : public MobEffect {
public:
    void onAdd(Actor &, const MobEffectInstance &) const override;
    void onRemove(Actor &, const MobEffectInstance &) const override;
    void tick(Actor &, const MobEffectInstance &) const override;
};

class PoisonMobEffect final : public MobEffect {
public:
    explicit PoisonMobEffect(bool fatal = false) : mFatal(fatal) {}
    void tick(Actor &, const MobEffectInstance &) const override;
    int32_t getApplyInterval(const MobEffectInstance &) const override;

private:
    bool mFatal;
};

class RegenerationMobEffect final : public MobEffect {
public:
    void tick(Actor &, const MobEffectInstance &) const override;
    int32_t getApplyInterval(const MobEffectInstance &) const override;
};

class SaturationMobEffect final : public InstantaneousMobEffect {
public:
    void tick(Actor &, const MobEffectInstance &) const override;
};

class SlownessMobEffect final : public MobEffect {};
class SpeedMobEffect final : public MobEffect {};
class WitherMobEffect final : public MobEffect {
public:
    void tick(Actor &, const MobEffectInstance &) const override;
    int32_t getApplyInterval(const MobEffectInstance &) const override;
};

// BDS names retained for effects whose gameplay hooks are not actor-local yet.
class AttackDamageMobEffect final : public MobEffect {};
class InfestedMobEffect final : public MobEffect {};
class OozingMobEffect final : public MobEffect {};
class RaidOmenMobEffect final : public MobEffect {};
class WeavingMobEffect final : public MobEffect {};
class WindChargedMobEffect final : public MobEffect {};

class MobEffectManager {
public:
    using EventCallback = std::function<void(const MobEffectInstance &, MobEffectEvent)>;
    using AddFilter = std::function<bool(Actor &, const MobEffectInstance &)>;

    explicit MobEffectManager(Actor &actor) : mActor(actor) {}

    static void setAddFilter(AddFilter filter) {
        sAddFilter = std::move(filter);
    }

    bool add(const MobEffectInstance &instance);
    bool remove(MobEffectId id);
    void clear();
    bool tick(int32_t tickDiff = 1);

    bool has(MobEffectId id) const;
    const MobEffectInstance *get(MobEffectId id) const;
    const std::unordered_map<int32_t, MobEffectInstance> &getAll() const { return mEffects; }

    void setEventCallback(EventCallback callback) { mEventCallback = std::move(callback); }

    float movementSpeedMultiplier() const;
    float jumpVelocityMultiplier() const;
    bool hasResistance() const { return has(MobEffectId::Resistance); }

    bool consumeAttributesDirty() {
        const bool dirty = mAttributesDirty;
        mAttributesDirty = false;
        return dirty;
    }

private:
    static std::unique_ptr<MobEffect> create(MobEffectId id);
    const MobEffect &effect(MobEffectId id) const;
    void refreshDerivedAttributes();
    void emit(const MobEffectInstance &instance, MobEffectEvent event) const;

    static inline AddFilter sAddFilter;

    Actor &mActor;
    std::unordered_map<int32_t, MobEffectInstance> mEffects;
    EventCallback mEventCallback;
    bool mAttributesDirty = false;
};

using MobEffectSystem = MobEffectManager;
