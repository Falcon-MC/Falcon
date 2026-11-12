#include "World/Actor/Projectile/ProjectileComponent.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"

namespace {
    // the loader accepts the snake_case key and an older spelling of it
    const json::Value *field(const json::Value &component, const char *key, const char *legacy) {
        const json::Value *value = component.get(key);
        return value != nullptr || legacy == nullptr ? value : component.get(legacy);
    }

    void readNumber(const json::Value &component, const char *key, const char *legacy, float &out) {
        const json::Value *value = field(component, key, legacy);
        if (value != nullptr && value->isNumber())
            out = (float) value->mNumber;
    }

    void readFlag(const json::Value &component, const char *key, const char *legacy, bool &out) {
        const json::Value *value = field(component, key, legacy);
        if (value != nullptr)
            out = value->boolean(out);
    }

    void readText(const json::Value &component, const char *key, const char *legacy, std::string &out) {
        const json::Value *value = field(component, key, legacy);
        if (value != nullptr && value->isString())
            out = value->mString;
    }
}

ProjectileComponent ProjectileComponent::read(const json::Value &component) {
    ProjectileComponent projectile;
    readNumber(component, "gravity", nullptr, projectile.mGravity);
    readNumber(component, "inertia", nullptr, projectile.mInertia);
    readNumber(component, "liquid_inertia", nullptr, projectile.mLiquidInertia);
    readNumber(component, "on_fire_time", "onFireTime", projectile.mOnFireTime);
    readFlag(component, "catch_fire", "catchFire", projectile.mCatchFire);
    readFlag(component, "crit_particle_on_hurt", "critParticleOnHurt", projectile.mCritParticleOnHurt);
    readFlag(component, "destroy_on_hurt", "destroyOnHurt", projectile.mDestroyOnHurt);
    readFlag(component, "lightning", nullptr, projectile.mLightning);
    readFlag(component, "stop_on_hurt", "stopOnHurt", projectile.mStopOnHurt);
    readText(component, "hit_sound", "hitSound", projectile.mHitSound);
    readText(component, "hit_ground_sound", "hitGroundSound", projectile.mHitGroundSound);
    readText(component, "particle", nullptr, projectile.mParticle);

    std::string bounce = "no";
    readText(component, "should_bounce", "shouldbounce", bounce);
    if (bounce == "if_invulnerable")
        projectile.mShouldBounce = Bounce::IfInvulnerable;
    else if (bounce == "if_no_damage_dealt")
        projectile.mShouldBounce = Bounce::IfNoDamageDealt;
    return projectile;
}

bool ProjectileComponent::find(const std::string &identifier, const CustomActorDefinition *custom,
                               ProjectileComponent &out) {
    if (custom != nullptr && custom->mHasProjectileComponent) {
        out = custom->mProjectileComponent;
        return true;
    }
    const json::Value *definition = EntityDefinitions::find(identifier);
    const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
    const json::Value *component = components == nullptr ? nullptr : components->get("minecraft:projectile");
    if (component == nullptr)
        return false;
    out = read(*component);
    return true;
}
