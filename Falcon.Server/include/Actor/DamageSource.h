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

class DamageSource {
public:
    static DamageSource environment(const std::string &deathMessageKey, const std::string &victimName) {
        DamageSource source;
        source.mDeathMessageKey = deathMessageKey;
        source.mDeathMessageParameters = {victimName};
        return source;
    }

    static DamageSource attack(const std::string &deathMessageKey, const std::string &victimName,
                               Actor &attacker, const std::string &attackerName, const Vector3f &origin) {
        DamageSource source;
        source.mDeathMessageKey = deathMessageKey;
        source.mDeathMessageParameters = {victimName, attackerName};
        source.mAttacker = &attacker;
        source.mOrigin = origin;
        return source;
    }

    DamageSource &withoutArmor() {
        mApplyArmor = false;
        return *this;
    }

    DamageSource &withoutCooldown() {
        mRespectCooldown = false;
        return *this;
    }

    DamageSource &fromOrigin(const Vector3f &origin) {
        mOrigin = origin;
        return *this;
    }

    DamageSource &asProjectile() {
        mProjectile = true;
        return *this;
    }

    DamageSource &disablingShield(bool disables) {
        mDisablesShield = disables;
        return *this;
    }

    DamageSource &withArmorEfficiency(float efficiency) {
        mArmorEfficiency = efficiency;
        return *this;
    }

    std::string mDeathMessageKey;
    std::vector<std::string> mDeathMessageParameters;
    Actor *mAttacker = nullptr;
    std::optional<Vector3f> mOrigin;
    bool mProjectile = false;
    bool mApplyArmor = true;
    bool mRespectCooldown = true;
    bool mDisablesShield = false;
    float mArmorEfficiency = 1.0f;
};
