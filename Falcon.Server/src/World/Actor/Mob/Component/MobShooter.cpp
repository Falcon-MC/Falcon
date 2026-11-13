#include "World/Actor/Mob/Component/MobShooter.h"

#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Item/PotionEffects.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"

#include <algorithm>
#include <cmath>
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
    // gaussian spread per uncertainty unit
    const float SPREAD_PER_UNCERTAINTY = 0.0075f;

    enum class Anchor { Feet, EyeHeight, Middle };

    struct ProjectileSettings {
        bool mFound = false;
        Anchor mAnchor = Anchor::Feet;
        float mPower = 0.0f;
        float mGravity = 0.0f;
        float mUncertaintyBase = 0.0f;
        float mUncertaintyMultiplier = 0.0f;
        Vector3f mOffset;
        const json::Value *mImpact = nullptr;
    };

    struct Shot {
        const json::Value *mIdentifier = nullptr;
        int32_t mAuxValue = -1;
        bool mLoseTarget = false;
    };

    std::mt19937 &shooterRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float numberOf(const json::Value &object, const char *key, float fallback) {
        const json::Value *value = object.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    int32_t auxOf(const json::Value &object) {
        const json::Value *value = object.get("aux_val");
        return value == nullptr ? -1 : value->integer(-1);
    }

    void readProjectile(const json::Value &component, ProjectileSettings &settings) {
        settings.mFound = true;
        const json::Value *anchor = component.get("anchor");
        const std::string anchorName = anchor == nullptr ? std::string() : anchor->string();
        settings.mAnchor = anchorName == "eye_height" ? Anchor::EyeHeight
                           : anchorName == "middle"   ? Anchor::Middle
                                                      : Anchor::Feet;
        settings.mPower = numberOf(component, "power", settings.mPower);
        settings.mGravity = numberOf(component, "gravity", settings.mGravity);
        settings.mUncertaintyBase = numberOf(component, "uncertainty_base", settings.mUncertaintyBase);
        settings.mUncertaintyMultiplier = numberOf(component, "uncertainty_multiplier",
                                                   settings.mUncertaintyMultiplier);
        const json::Value *offset = component.get("offset");
        if (offset != nullptr && offset->isArray() && offset->mArray.size() == 3)
            settings.mOffset = Vector3f((float) offset->mArray[0]->number(0.0), (float) offset->mArray[1]->number(0.0),
                                        (float) offset->mArray[2]->number(0.0));

        const json::Value *onHit = component.get("on_hit");
        settings.mImpact = onHit == nullptr ? nullptr : onHit->get("impact_damage");
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

    // a group replaces the whole component, as component groups do
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

    // first entry of projectiles whose chance and filters pass, the target as other; else def
    Shot chooseShot(ServerNetworkHandler &owner, const MobActor &mob, const Actor &target, const json::Value &shooter) {
        Shot shot;
        shot.mIdentifier = shooter.get("def");
        shot.mAuxValue = auxOf(shooter);

        const json::Value *projectiles = shooter.get("projectiles");
        if (projectiles == nullptr || !projectiles->isArray())
            return shot;

        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        for (const std::unique_ptr<json::Value> &entry: projectiles->mArray) {
            if (numberOf(*entry, "chance", 1.0f) <= roll(shooterRandom()))
                continue;

            const json::Value *filters = entry->get("filters");
            if (filters != nullptr && !EntityFilter::test(*filters, owner, mob, &target))
                continue;

            shot.mIdentifier = entry->get("def");
            shot.mAuxValue = auxOf(*entry);
            const json::Value *loseTarget = entry->get("lose_target");
            shot.mLoseTarget = loseTarget != nullptr && loseTarget->boolean(false);
            return shot;
        }
        return shot;
    }

    float heightOf(const Actor &actor) {
        const ServerActor *server = dynamic_cast<const ServerActor *>(&actor);
        return server == nullptr ? PLAYER_HEIGHT : server->getSize().mHeight;
    }

    float anchorHeight(Anchor anchor, float height) {
        if (anchor == Anchor::EyeHeight)
            return height * EYE_HEIGHT_RATIO;
        return anchor == Anchor::Middle ? height * 0.5f : 0.0f;
    }

    Vector3f directionOf(float pitch, float yaw) {
        const float pitchRadians = pitch * DEGREES_TO_RADIANS;
        const float yawRadians = yaw * DEGREES_TO_RADIANS;
        const float horizontal = std::cos(pitchRadians);
        return Vector3f(-std::sin(yawRadians) * horizontal, -std::sin(pitchRadians), std::cos(yawRadians) * horizontal);
    }

    void applyAux(ServerNetworkHandler &owner, ServerActor &projectile, int32_t auxValue) {
        if (auxValue < 0)
            return;

        // a splash potion's aux is the potion id; an arrow's is potion id + 1, as the arrow item's damage
        if (PotionActor *potion = dynamic_cast<PotionActor *>(&projectile))
            potion->setPotionId(auxValue);
        else if (dynamic_cast<ArrowActor *>(&projectile) != nullptr && auxValue > 0)
            projectile.getProjectileData().mPotionId = auxValue - 1;
        else
            return;

        // viewers draw the potion color from it
        EntityDataEntry entry;
        entry.mId = ACTOR_DATA_POTION_AUX_VALUE;
        entry.mFormat = EntityDataFormat::Short;
        entry.mShortValue = (int16_t) auxValue;
        EntityDataMap metadata;
        metadata.mEntries.push_back(entry);
        owner.sendActorMetadata(projectile, metadata);
    }
}

bool MobShooter::shootAt(ServerNetworkHandler &owner, MobActor &mob, const Actor &target) {
    const json::Value *shooter = mob.getComponent(SHOOTER_COMPONENT);
    if (shooter == nullptr)
        return false;

    const Shot shot = chooseShot(owner, mob, target, *shooter);
    if (shot.mIdentifier == nullptr)
        return false;

    const std::string identifier = shot.mIdentifier->string();
    const ProjectileSettings settings = projectileSettings(owner, mob, identifier);
    if (!settings.mFound)
        return false;

    // the shooter's own power wins over the projectile's when set
    const float shooterPower = numberOf(*shooter, "power", 0.0f);
    const float power = shooterPower > 0.0f ? shooterPower : settings.mPower;

    // anchor plus the offset turned with the body yaw
    const Vector3f position = mob.getPosition();
    const float bodyYaw = mob.getRotation().y * -DEGREES_TO_RADIANS;
    const float cosine = std::cos(bodyYaw);
    const float sine = std::sin(bodyYaw);
    const Vector3f origin(position.x + settings.mOffset.z * sine + settings.mOffset.x * cosine,
                          position.y + anchorHeight(settings.mAnchor, mob.getSize().mHeight) + settings.mOffset.y,
                          position.z + settings.mOffset.z * cosine - settings.mOffset.x * sine);

    const Vector3f targetPosition = target.getPosition();
    const float dx = targetPosition.x - origin.x;
    const float dy = targetPosition.y + heightOf(target) * 0.5f - origin.y;
    const float dz = targetPosition.z - origin.z;
    const float horizontal = std::sqrt(dx * dx + dz * dz);
    if (horizontal < 0.0000001f)
        return false;

    const float yaw = std::atan2(dz, dx) * RADIANS_TO_DEGREES - 90.0f;
    Vector3f direction = directionOf(aimPitch(horizontal, dy, power, settings.mGravity), yaw);

    const float spread = SPREAD_PER_UNCERTAINTY
                         * uncertainty(settings.mUncertaintyBase, settings.mUncertaintyMultiplier,
                                       (int32_t) owner.getProperties().getDifficulty());
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    direction.x += gaussian(shooterRandom()) * spread;
    direction.y += gaussian(shooterRandom()) * spread;
    direction.z += gaussian(shooterRandom()) * spread;

    // spawned a block ahead of the anchor, toward the target
    const Vector3f spawn(origin.x + dx / horizontal, origin.y, origin.z + dz / horizontal);
    ServerActor *projectile = owner.spawnActor(owner.getLevelFor(mob), identifier, spawn);
    if (projectile == nullptr)
        return false;

    projectile->setProjectile(true);
    projectile->setOwnerUniqueId((int64_t) mob.getRuntimeId());
    if (settings.mImpact != nullptr) {
        ProjectileData &data = projectile->getProjectileData();
        data.mImpactDamage = impactDamage(*settings.mImpact, shooterRandom());
        if (data.mImpactDamage < 0)
            data.mBaseDamage = numberOf(*settings.mImpact, "power_multiplier", data.mBaseDamage);
    }

    projectile->setMotion(Vector3f(direction.x * power, direction.y * power, direction.z * power));
    if (!owner.allowProjectileLaunch(*projectile, &mob))
        return false;

    applyAux(owner, *projectile, shot.mAuxValue);

    const json::Value *sound = shooter->get("sound");
    if (sound != nullptr && !sound->string().empty())
        owner.playLevelSound(owner.getLevelFor(mob), sound->string(), position, mob.getIdentifier());

    if (shot.mLoseTarget)
        mob.clearTarget();
    return true;
}

float MobShooter::aimPitch(float horizontal, float height, float power, float gravity) {
    if (gravity == 0.0f)
        return -std::atan2(height, horizontal) * RADIANS_TO_DEGREES;

    const float squared = power * power;
    const float discriminant = squared * squared
                               - gravity * (2.0f * height * squared + horizontal * horizontal * gravity);
    if (discriminant < 0.0f)
        return UNREACHABLE_PITCH;

    const float root = std::sqrt(discriminant);
    const float high = std::atan2(squared + root, horizontal * gravity) * RADIANS_TO_DEGREES;
    const float low = std::atan2(squared - root, horizontal * gravity) * RADIANS_TO_DEGREES;
    return -std::min(high, low);
}

// uncertainty_base - difficulty * uncertainty_multiplier, never negative
float MobShooter::uncertainty(float base, float multiplier, int32_t difficulty) {
    return std::max(base - (float) difficulty * multiplier, 0.0f);
}

int32_t MobShooter::impactDamage(const json::Value &impact, std::mt19937 &random) {
    const json::Value *damage = impact.get("damage");
    if (damage == nullptr)
        return -1;

    if (!damage->isObject())
        return damage->integer(0) > 0 ? damage->integer(0) : -1;

    const int32_t minimum = std::max(0, (int32_t) numberOf(*damage, "min", 0.0f));
    const int32_t maximum = std::max(minimum, (int32_t) numberOf(*damage, "max", (float) minimum));
    if (maximum <= 0)
        return -1;
    return std::uniform_int_distribution<int32_t>(minimum, maximum)(random);
}
