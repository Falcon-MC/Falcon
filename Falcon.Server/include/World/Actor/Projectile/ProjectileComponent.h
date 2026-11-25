#pragma once

#include "Core/Json/Json.h"
#include "World/Level/LevelSoundEventIds.h"

#include <cstdint>
#include <string>

class Actor;
class ServerActor;
class ServerNetworkHandler;
struct CustomActorDefinition;

// minecraft:projectile as one entity carries it; scripts read and change it through EntityProjectileComponent
struct ProjectileComponent {
    // the loader's default particle
    static constexpr int32_t PARTICLE_ICON_CRACK = 14;

    // should_bounce: "no", "if_invulnerable", "if_no_damage_dealt"
    enum class Bounce : uint8_t { No = 0, IfInvulnerable = 1, IfNoDamageDealt = 2 };

    // defaults of the definition loader
    float mGravity = 0.05f;
    float mInertia = 0.99f;
    float mLiquidInertia = 0.6f;
    float mOnFireTime = 5.0f;
    float mPower = 1.3f;
    // degrees added to the shooter's pitch at launch, negative aims higher
    float mAngleOffset = 0.0f;
    // the launch spread: uncertainty_base less difficulty times uncertainty_multiplier
    float mUncertaintyBase = 0.0f;
    float mUncertaintyMultiplier = 0.0f;
    bool mCatchFire = false;
    bool mCritParticleOnHurt = false;
    bool mDestroyOnHurt = false;
    bool mLightning = false;
    bool mStopOnHurt = false;
    // steers toward the shooter's target instead of flying straight
    bool mHoming = false;
    // a melee hit sends it back where the attacker looks, once reflect_immunity has passed
    bool mReflectOnHurt = false;
    float mReflectImmunity = 0.0f;
    Bounce mShouldBounce = Bounce::No;
    // LevelSoundEvent numbers, UNDEFINED for none
    int32_t mHitSound = LevelSoundEventIds::UNDEFINED;
    int32_t mHitGroundSound = LevelSoundEventIds::UNDEFINED;
    // ParticleType; 0 has no name, scripts read it as undefined
    int32_t mParticle = PARTICLE_ICON_CRACK;

    static ProjectileComponent read(const json::Value &component);

    // ParticleType by its exact name, 0 for any other name
    static int32_t particleType(const std::string &name);

    // the name of a ParticleType, empty for 0 or an unknown type
    static const char *particleName(int32_t type);

    // the entity's own definition: a behavior pack one first, else the vanilla one; false when it has none
    static bool find(const std::string &identifier, const CustomActorDefinition *custom, ProjectileComponent &out);

    // as find, after the projectile's minecraft:entity_spawned event added its group for that shooter: arrow.json
    // player_arrow, player_crossbow_arrow
    static bool findSpawned(ServerNetworkHandler &owner, const ServerActor &projectile, const Actor &shooter,
                            ProjectileComponent &out);

    // the group minecraft:entity_spawned adds to the definition, the shooter as other; null when none
    static const json::Value *spawnedGroup(ServerNetworkHandler &owner, const json::Value &definition,
                                           const ServerActor &self, const Actor &shooter);
};
