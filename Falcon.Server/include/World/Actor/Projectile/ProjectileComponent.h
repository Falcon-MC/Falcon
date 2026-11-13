#pragma once

#include "Core/Json/Json.h"
#include "World/Level/LevelSoundEventIds.h"

#include <cstdint>
#include <string>

struct CustomActorDefinition;

// minecraft:projectile as one entity carries it; scripts read and change it through EntityProjectileComponent
struct ProjectileComponent {
    // ParticleType::IconCrack, the loader's default particle (sub_143CADDC0, 26.60)
    static constexpr int32_t PARTICLE_ICON_CRACK = 14;

    // should_bounce: "no", "if_invulnerable", "if_no_damage_dealt" (sub_143CADDC0, 26.60)
    enum class Bounce : uint8_t { No = 0, IfInvulnerable = 1, IfNoDamageDealt = 2 };

    // defaults of the definition loader: sub_143CADDC0 (26.60)
    float mGravity = 0.05f;
    float mInertia = 0.99f;
    float mLiquidInertia = 0.6f;
    float mOnFireTime = 5.0f;
    float mPower = 1.3f;
    float mUncertaintyBase = 0.0f;
    bool mCatchFire = false;
    bool mCritParticleOnHurt = false;
    bool mDestroyOnHurt = false;
    bool mLightning = false;
    bool mStopOnHurt = false;
    Bounce mShouldBounce = Bounce::No;
    // LevelSoundEvent numbers, UNDEFINED for none
    int32_t mHitSound = LevelSoundEventIds::UNDEFINED;
    int32_t mHitGroundSound = LevelSoundEventIds::UNDEFINED;
    // ParticleType; 0 has no name, scripts read it as undefined
    int32_t mParticle = PARTICLE_ICON_CRACK;

    static ProjectileComponent read(const json::Value &component);

    // ParticleType by its exact name, 0 for any other name (sub_141D318C0, 26.60)
    static int32_t particleType(const std::string &name);

    // the name of a ParticleType, empty for 0 or an unknown type (sub_141D304A0, 26.60)
    static const char *particleName(int32_t type);

    // the entity's own definition: a behavior pack one first, else the vanilla one; false when it has none
    static bool find(const std::string &identifier, const CustomActorDefinition *custom, ProjectileComponent &out);
};
