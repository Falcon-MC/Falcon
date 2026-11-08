#pragma once

#include "Core/Math/Vector3f.h"

#include <optional>
#include <string>
#include <vector>

class Actor;

enum class DamageResult {
    Ignored,
    Blocked,
    Dealt
};

class ActorDamageSource {
public:
    static ActorDamageSource environment(const std::string &deathMessageKey, const std::string &victimName) {
        ActorDamageSource source;
        source.mDeathMessageKey = deathMessageKey;
        source.mDeathMessageParameters = {victimName};
        return source;
    }

    static ActorDamageSource attack(const std::string &deathMessageKey, const std::string &victimName,
                               Actor &attacker, const std::string &attackerName, const Vector3f &origin) {
        ActorDamageSource source;
        source.mDeathMessageKey = deathMessageKey;
        source.mDeathMessageParameters = {victimName, attackerName};
        source.mAttacker = &attacker;
        source.mOrigin = origin;
        return source;
    }

    ActorDamageSource &withoutArmor() {
        mApplyArmor = false;
        return *this;
    }

    ActorDamageSource &withoutCooldown() {
        mRespectCooldown = false;
        return *this;
    }

    ActorDamageSource &fromOrigin(const Vector3f &origin) {
        mOrigin = origin;
        return *this;
    }

    ActorDamageSource &asProjectile() {
        mProjectile = true;
        return *this;
    }

    ActorDamageSource &disablingShield(bool disables) {
        mDisablesShield = disables;
        return *this;
    }

    ActorDamageSource &withArmorEfficiency(float efficiency) {
        mArmorEfficiency = efficiency;
        return *this;
    }

    // a cause the death message does not tell: minecraft:area_attack overrides the message of its cause
    ActorDamageSource &withCause(const std::string &cause) {
        mCause = cause;
        return *this;
    }

    ActorDamageSource &withDamager(Actor *damager) {
        mDamager = damager;
        return *this;
    }

    std::string mDeathMessageKey;
    std::string mCause;
    std::vector<std::string> mDeathMessageParameters;
    Actor *mAttacker = nullptr;
    Actor *mDamager = nullptr;
    std::optional<Vector3f> mOrigin;
    bool mProjectile = false;
    bool mApplyArmor = true;
    bool mRespectCooldown = true;
    bool mDisablesShield = false;
    float mArmorEfficiency = 1.0f;
};
