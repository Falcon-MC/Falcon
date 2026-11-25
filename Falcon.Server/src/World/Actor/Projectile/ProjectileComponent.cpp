#include "World/Actor/Projectile/ProjectileComponent.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/ServerActor.h"


namespace {
    struct ParticleTypeName {
        const char *mName;
        int32_t mType;
    };

    // the ParticleType names the game reflects
    const ParticleTypeName PARTICLE_TYPES[] = {{"none", 0},
                                               {"bubble", 1},
                                               {"bubblemanual", 2},
                                               {"crit", 3},
                                               {"blockforcefield", 4},
                                               {"smoke", 5},
                                               {"explode", 6},
                                               {"evaporation", 7},
                                               {"flame", 8},
                                               {"candleflame", 9},
                                               {"lava", 10},
                                               {"largesmoke", 11},
                                               {"reddust", 12},
                                               {"risingborderdust", 13},
                                               {"iconcrack", 14},
                                               {"snowballpoof", 15},
                                               {"largeexplode", 16},
                                               {"hugeexplosion", 17},
                                               {"breezewindexplosion", 18},
                                               {"mobflame", 19},
                                               {"heart", 20},
                                               {"terrain", 21},
                                               {"townaura", 22},
                                               {"portal", 23},
                                               {"watersplash", 25},
                                               {"watersplashmanual", 26},
                                               {"waterwake", 27},
                                               {"dripwater", 28},
                                               {"driplava", 29},
                                               {"driphoney", 30},
                                               {"stalactitedripwater", 31},
                                               {"stalactitedriplava", 32},
                                               {"fallingdust", 33},
                                               {"mobspell", 34},
                                               {"mobspellambient", 35},
                                               {"mobspellinstantaneous", 36},
                                               {"ink", 37},
                                               {"slime", 38},
                                               {"rainsplash", 39},
                                               {"villagerangry", 40},
                                               {"villagerhappy", 41},
                                               {"enchantingtable", 42},
                                               {"trackingemitter", 43},
                                               {"note", 44},
                                               {"witchspell", 45},
                                               {"carrotboost", 46},
                                               {"mobappearance", 47},
                                               {"endrod", 48},
                                               {"dragonbreath", 49},
                                               {"spit", 50},
                                               {"totem", 51},
                                               {"food", 52},
                                               {"fireworksstarter", 53},
                                               {"fireworks", 54},
                                               {"fireworksoverlay", 55},
                                               {"balloongas", 56},
                                               {"coloredflame", 57},
                                               {"sparkler", 58},
                                               {"conduit", 59},
                                               {"bubblecolumnup", 60},
                                               {"bubblecolumndown", 61},
                                               {"sneeze", 62},
                                               {"shulkerbullet", 63},
                                               {"bleach", 64},
                                               {"dragondestroyblock", 65},
                                               {"myceliumdust", 66},
                                               {"fallingborderdust", 67},
                                               {"campfiresmoke", 68},
                                               {"campfiresmoketall", 69},
                                               {"dragonbreathfire", 70},
                                               {"dragonbreathtrail", 71},
                                               {"soul", 73},
                                               {"obsidiantear", 74},
                                               {"portalreverse", 75},
                                               {"snowflake", 76},
                                               {"wax", 81},
                                               {"electricspark", 82},
                                               {"shriek", 83},
                                               {"sculksoul", 84},
                                               {"sonicexplosion", 85},
                                               {"dustplume", 88},
                                               {"whitesmoke", 89},
                                               {"wolfarmorcrack", 92},
                                               {"creakingcrumble", 94},
                                               {"paleoakleaves", 95},
                                               {"eyeblossomopen", 96},
                                               {"eyeblossomclose", 97},
                                               {"blueflame", 72},
                                               {"greenflame", 98},
                                               {"pausemobgrowth", 99},
                                               {"resetmobgrowth", 100},
                                               {"sulfurcube", 101},
                                               {"orangepoplarleaves", 102},
                                               {"redpoplarleaves", 103},
                                               {"yellowpoplarleaves", 104},
                                               {"iceballbreak", 105}

    };

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
    readNumber(component, "power", nullptr, projectile.mPower);
    readNumber(component, "angle_offset", "angleoffset", projectile.mAngleOffset);
    readNumber(component, "uncertainty_base", "uncertaintyBase", projectile.mUncertaintyBase);
    readNumber(component, "uncertainty_multiplier", "uncertaintyMultiplier", projectile.mUncertaintyMultiplier);
    readFlag(component, "catch_fire", "catchFire", projectile.mCatchFire);
    readFlag(component, "crit_particle_on_hurt", "critParticleOnHurt", projectile.mCritParticleOnHurt);
    readFlag(component, "destroy_on_hurt", "destroyOnHurt", projectile.mDestroyOnHurt);
    readFlag(component, "lightning", nullptr, projectile.mLightning);
    readFlag(component, "stop_on_hurt", "stopOnHurt", projectile.mStopOnHurt);
    readFlag(component, "homing", nullptr, projectile.mHoming);
    readFlag(component, "reflect_on_hurt", "reflectOnHurt", projectile.mReflectOnHurt);
    readNumber(component, "reflect_immunity", nullptr, projectile.mReflectImmunity);
    // a sound name the game does not know becomes the undefined sound event
    std::string sound;
    readText(component, "hit_sound", "hitSound", sound);
    projectile.mHitSound = sound.empty() ? LevelSoundEventIds::UNDEFINED : LevelSoundEventIds::fromName(sound);
    sound.clear();
    readText(component, "hit_ground_sound", "hitGroundSound", sound);
    projectile.mHitGroundSound = sound.empty() ? LevelSoundEventIds::UNDEFINED : LevelSoundEventIds::fromName(sound);
    // an unknown name leaves ParticleType 0
    std::string particle = ProjectileComponent::particleName(ProjectileComponent::PARTICLE_ICON_CRACK);
    readText(component, "particle", nullptr, particle);
    projectile.mParticle = ProjectileComponent::particleType(particle);

    std::string bounce = "no";
    readText(component, "should_bounce", "shouldbounce", bounce);
    if (bounce == "if_invulnerable")
        projectile.mShouldBounce = Bounce::IfInvulnerable;
    else if (bounce == "if_no_damage_dealt")
        projectile.mShouldBounce = Bounce::IfNoDamageDealt;
    return projectile;
}

int32_t ProjectileComponent::particleType(const std::string &name) {
    for (const ParticleTypeName &entry: PARTICLE_TYPES) {
        if (name == entry.mName)
            return entry.mType;
    }
    return 0;
}

const char *ProjectileComponent::particleName(int32_t type) {
    if (type == 0)
        return "";
    for (const ParticleTypeName &entry: PARTICLE_TYPES) {
        if (entry.mType == type)
            return entry.mName;
    }
    return "";
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

const json::Value *ProjectileComponent::spawnedGroup(ServerNetworkHandler &owner, const json::Value &definition,
                                                     const ServerActor &self, const Actor &shooter) {
    const json::Value *events = definition.get("events");
    const json::Value *spawned = events == nullptr ? nullptr : events->get("minecraft:entity_spawned");
    const json::Value *choices = spawned == nullptr ? nullptr : spawned->get("first_valid");
    const json::Value *groups = definition.get("component_groups");
    if (choices == nullptr || !choices->isArray() || groups == nullptr)
        return nullptr;

    for (const std::unique_ptr<json::Value> &choice: choices->mArray) {
        const json::Value *filters = choice->get("filters");
        if (filters != nullptr && !EntityFilter::test(*filters, owner, self, &shooter))
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
bool ProjectileComponent::findSpawned(ServerNetworkHandler &owner, const ServerActor &projectile, const Actor &shooter,
                                      ProjectileComponent &out) {
    const CustomActorDefinition *custom = projectile.getDefinition();
    const std::string identifier = projectile.getIdentifier();
    if (custom != nullptr && custom->mHasProjectileComponent)
        return find(identifier, custom, out);

    const json::Value *definition = EntityDefinitions::find(identifier);
    const json::Value *group = definition == nullptr ? nullptr : spawnedGroup(owner, *definition, projectile, shooter);
    const json::Value *component = group == nullptr ? nullptr : group->get("minecraft:projectile");
    if (component == nullptr)
        return find(identifier, custom, out);
    out = read(*component);
    return true;
}
