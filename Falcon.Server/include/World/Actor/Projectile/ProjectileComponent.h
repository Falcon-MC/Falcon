#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <string>

struct CustomActorDefinition;

// minecraft:projectile as one entity carries it; scripts read and change it through EntityProjectileComponent
struct ProjectileComponent {
    // should_bounce: "no", "if_invulnerable", "if_no_damage_dealt" (sub_143CADDC0, 26.60)
    enum class Bounce : uint8_t { No = 0, IfInvulnerable = 1, IfNoDamageDealt = 2 };

    // defaults of the definition loader: sub_143CADDC0 (26.60)
    float mGravity = 0.05f;
    float mInertia = 0.99f;
    float mLiquidInertia = 0.6f;
    float mOnFireTime = 5.0f;
    bool mCatchFire = false;
    bool mCritParticleOnHurt = false;
    bool mDestroyOnHurt = false;
    bool mLightning = false;
    bool mStopOnHurt = false;
    Bounce mShouldBounce = Bounce::No;
    std::string mHitSound;
    std::string mHitGroundSound;
    // ParticleType by name; "none" is ParticleType 0, which scripts read as undefined
    std::string mParticle = "iconcrack";

    static ProjectileComponent read(const json::Value &component);

    // the entity's own definition: a behavior pack one first, else the vanilla one; false when it has none
    static bool find(const std::string &identifier, const CustomActorDefinition *custom, ProjectileComponent &out);
};
