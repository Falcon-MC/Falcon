#include "World/Actor/Mob/Component/MobShooter.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Item/PotionEffects.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace {
    const char *const SHOOTER_COMPONENT = "minecraft:shooter";
    const char *const PROJECTILE_COMPONENT = "minecraft:projectile";
    const char *const SPAWNED_EVENT = "minecraft:entity_spawned";
    const float EYE_HEIGHT_RATIO = 0.85f;
    const float PLAYER_HEIGHT = 1.8f;
    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float RADIANS_TO_DEGREES = 57.295776f;
    // no ballistic solution in reach: the game shoots 45 degrees up
    const float UNREACHABLE_PITCH = -45.0f;
    // gaussian spread per uncertainty unit: sub_143CB38D0
    const float SPREAD_PER_UNCERTAINTY = 0.0075f;

    struct ProjectileSettings {
        bool mFound = false;
        float mPower = 0.0f;
        float mGravity = 0.0f;
        float mUncertaintyBase = 0.0f;
        float mUncertaintyMultiplier = 0.0f;
        float mOffsetY = 0.0f;
        float mDamageMultiplier = 0.0f;
    };

    std::mt19937 &shooterRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float numberIn(const json::Value &object, const char *key, float fallback) {
        const json::Value *value = object.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    void readProjectile(const json::Value &component, ProjectileSettings &settings) {
        settings.mFound = true;
        settings.mPower = numberIn(component, "power", settings.mPower);
        settings.mGravity = numberIn(component, "gravity", settings.mGravity);
        settings.mUncertaintyBase = numberIn(component, "uncertainty_base", settings.mUncertaintyBase);
        settings.mUncertaintyMultiplier = numberIn(component, "uncertainty_multiplier",
                                                   settings.mUncertaintyMultiplier);
        const json::Value *offset = component.get("offset");
        if (offset != nullptr && offset->isArray() && offset->mArray.size() == 3)
            settings.mOffsetY = (float) offset->mArray[1]->number(0.0);

        const json::Value *onHit = component.get("on_hit");
        const json::Value *impact = onHit == nullptr ? nullptr : onHit->get("impact_damage");
        if (impact != nullptr)
            settings.mDamageMultiplier = numberIn(*impact, "power_multiplier", settings.mDamageMultiplier);
    }

    // the projectile's own spawn event picks its group from the shooter: arrow.json mob_arrow, mob_crossbow_arrow
    const json::Value *spawnedGroup(ServerNetworkHandler &owner, const MobActor &mob, const json::Value &definition) {
        const json::Value *events = definition.get("events");
        const json::Value *spawned = events == nullptr ? nullptr : events->get(SPAWNED_EVENT);
        const json::Value *choices = spawned == nullptr ? nullptr : spawned->get("first_valid");
        const json::Value *groups = definition.get("component_groups");
        if (choices == nullptr || !choices->isArray() || groups == nullptr)
            return nullptr;

        for (const std::unique_ptr<json::Value> &choice: choices->mArray) {
            const json::Value *filters = choice->get("filters");
            if (filters != nullptr && !EntityFilter::test(*filters, owner, mob, &mob))
                continue;

            const json::Value *add = choice->get("add");
            const json::Value *names = add == nullptr ? nullptr : add->get("component_groups");
            if (names == nullptr || !names->isArray() || names->mArray.empty())
                return nullptr;

            return groups->get(names->mArray.front()->string());
        }
        return nullptr;
    }

    ProjectileSettings projectileSettings(ServerNetworkHandler &owner, const MobActor &mob,
                                          const std::string &identifier) {
        ProjectileSettings settings;
        const json::Value *definition = EntityDefinitions::find(identifier);
        if (definition == nullptr)
            return settings;

        const json::Value *components = definition->get("components");
        const json::Value *base = components == nullptr ? nullptr : components->get(PROJECTILE_COMPONENT);
        if (base != nullptr)
            readProjectile(*base, settings);

        const json::Value *group = spawnedGroup(owner, mob, *definition);
        const json::Value *override = group == nullptr ? nullptr : group->get(PROJECTILE_COMPONENT);
        if (override != nullptr) {
            settings = ProjectileSettings();
            readProjectile(*override, settings);
        }
        return settings;
    }

    float heightOf(const Actor &actor) {
        const ServerActor *server = dynamic_cast<const ServerActor *>(&actor);
        return server == nullptr ? PLAYER_HEIGHT : server->getSize().mHeight;
    }

    Vector3f directionOf(float pitch, float yaw) {
        const float pitchRadians = pitch * DEGREES_TO_RADIANS;
        const float yawRadians = yaw * DEGREES_TO_RADIANS;
        const float horizontal = std::cos(pitchRadians);
        return Vector3f(-std::sin(yawRadians) * horizontal, -std::sin(pitchRadians),
                        std::cos(yawRadians) * horizontal);
    }
}

bool MobShooter::canShoot(const MobActor &mob) {
    const json::Value *shooter = mob.getComponent(SHOOTER_COMPONENT);
    const json::Value *identifier = shooter == nullptr ? nullptr : shooter->get("def");
    return identifier != nullptr && ActorClassRegistry::getPrototype(identifier->string()) != nullptr;
}

bool MobShooter::shootAt(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) {
    const json::Value *shooter = mob.getComponent(SHOOTER_COMPONENT);
    const json::Value *def = shooter == nullptr ? nullptr : shooter->get("def");
    if (def == nullptr)
        return false;

    const std::string identifier = def->string();
    const ProjectileSettings settings = projectileSettings(owner, mob, identifier);
    if (!settings.mFound)
        return false;

    const Vector3f position = mob.getPosition();
    const Vector3f origin(position.x, position.y + mob.getSize().mHeight * EYE_HEIGHT_RATIO + settings.mOffsetY,
                          position.z);
    const Vector3f targetPosition = target.getPosition();
    const float dx = targetPosition.x - origin.x;
    const float dy = targetPosition.y + heightOf(target) * 0.5f - origin.y;
    const float dz = targetPosition.z - origin.z;
    const float horizontal = std::sqrt(dx * dx + dz * dz);
    if (horizontal < 0.0000001f)
        return false;

    const float yaw = std::atan2(dz, dx) * RADIANS_TO_DEGREES - 90.0f;
    const float pitch = aimPitch(horizontal, dy, settings.mPower, settings.mGravity);
    Vector3f direction = directionOf(pitch, yaw);

    const float spread = SPREAD_PER_UNCERTAINTY * uncertainty(settings.mUncertaintyBase,
                                                              settings.mUncertaintyMultiplier,
                                                              (int32_t) owner.getProperties().getDifficulty());
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    direction.x += gaussian(shooterRandom()) * spread;
    direction.y += gaussian(shooterRandom()) * spread;
    direction.z += gaussian(shooterRandom()) * spread;

    ServerActor *projectile = owner.spawnActor(owner.getLevelFor(mob), identifier, origin);
    if (projectile == nullptr)
        return false;

    projectile->setProjectile(true);
    projectile->setOwnerUniqueId((int64_t) mob.getRuntimeId());
    ProjectileData &data = projectile->getProjectileData();
    data.mBaseDamage = settings.mDamageMultiplier;
    // shooter aux_val is the tipped arrow aux: potion id + 1, as the arrow item's damage
    const json::Value *auxJson = shooter->get("aux_val");
    const int32_t auxValue = auxJson == nullptr ? 0 : auxJson->integer(0);
    if (auxValue > 0)
        data.mPotionId = auxValue - 1;

    projectile->setMotion(Vector3f(direction.x * settings.mPower, direction.y * settings.mPower,
                                   direction.z * settings.mPower));
    if (!owner.allowProjectileLaunch(*projectile, &mob))
        return false;

    // the game hands aux_val to the projectile: viewers draw the tipped arrow from it (sub_14299A250)
    if (auxValue > 0) {
        EntityDataEntry entry;
        entry.mId = ACTOR_DATA_POTION_AUX_VALUE;
        entry.mFormat = EntityDataFormat::Short;
        entry.mShortValue = (int16_t) auxValue;
        EntityDataMap metadata;
        metadata.mEntries.push_back(entry);
        owner.sendActorMetadata(*projectile, metadata);
    }

    const json::Value *sound = shooter->get("sound");
    if (sound != nullptr && !sound->string().empty())
        owner.playLevelSound(owner.getLevelFor(mob), sound->string(), position, mob.getIdentifier());
    return true;
}

// sub_14299A250
float MobShooter::aimPitch(float horizontal, float height, float power, float gravity) {
    if (gravity == 0.0f)
        return -std::atan2(height, horizontal) * RADIANS_TO_DEGREES;

    const float squared = power * power;
    const float discriminant = squared * squared - gravity * (2.0f * height * squared
                                                               + horizontal * horizontal * gravity);
    if (discriminant < 0.0f)
        return UNREACHABLE_PITCH;

    const float root = std::sqrt(discriminant);
    const float high = std::atan2(squared + root, horizontal * gravity) * RADIANS_TO_DEGREES;
    const float low = std::atan2(squared - root, horizontal * gravity) * RADIANS_TO_DEGREES;
    return -std::min(high, low);
}

// uncertainty_base - difficulty * uncertainty_multiplier, never negative: sub_143CB3740
float MobShooter::uncertainty(float base, float multiplier, int32_t difficulty) {
    return std::max(base - (float) difficulty * multiplier, 0.0f);
}
