#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/RangedAttackGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Component/MobShooter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <cmath>
#include <initializer_list>
#include <random>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool closeTo(float value, float expected, float tolerance) {
        return std::fabs(value - expected) <= tolerance;
    }

    const json::Value &path(const json::Value *root, std::initializer_list<const char *> keys) {
        const json::Value *value = root;
        for (const char *key: keys) {
            require(value != nullptr, key);
            value = value->get(key);
        }
        require(value != nullptr, "missing definition field");
        return *value;
    }

    void aimPitch() {
        require(MobShooter::aimPitch(10.0f, 0.0f, 1.6f, 0.0f) == 0.0f, "no gravity aims straight");
        // mob_arrow: power 1.6, gravity 0.05, level 10 blocks away
        require(closeTo(MobShooter::aimPitch(10.0f, 0.0f, 1.6f, 0.05f), -5.63f, 0.02f), "low arc at 10 blocks");
        require(MobShooter::aimPitch(10.0f, 2.0f, 1.6f, 0.05f) < MobShooter::aimPitch(10.0f, 0.0f, 1.6f, 0.05f),
                "a higher target needs a steeper arc");
        require(MobShooter::aimPitch(100.0f, 0.0f, 1.6f, 0.05f) == -45.0f, "out of reach shoots 45 degrees up");
    }

    void uncertainty() {
        // mob_arrow uncertainty_base 16, uncertainty_multiplier 4
        require(MobShooter::uncertainty(16.0f, 4.0f, 0) == 16.0f, "peaceful");
        require(MobShooter::uncertainty(16.0f, 4.0f, 1) == 12.0f, "easy");
        require(MobShooter::uncertainty(16.0f, 4.0f, 2) == 8.0f, "normal");
        require(MobShooter::uncertainty(16.0f, 4.0f, 3) == 4.0f, "hard");
        require(MobShooter::uncertainty(1.0f, 4.0f, 3) == 0.0f, "never negative");
    }

    // an empty overworld patch: mobs see each other through air
    struct Field {
        ServerNetworkHandler mOwner{"Ranged mobs", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Field() {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, identifier);
            return *mob;
        }

        template <typename T>
        std::vector<T *> all() {
            std::vector<T *> found;
            for (auto &entry: mOwner.getActors()) {
                if (T *actor = dynamic_cast<T *>(entry.second.get()))
                    found.push_back(actor);
            }
            return found;
        }
    };

    float speedOf(const ServerActor &actor) {
        const Vector3f motion = actor.getMotion();
        return std::sqrt(motion.x * motion.x + motion.y * motion.y + motion.z * motion.z);
    }

    // 60 tick interval: the first shot lands on tick 62 and the next 61 ticks later (sub_14D429E10)
    void attackTimeline() {
        Field field;
        MobActor &skeleton = field.spawn("minecraft:skeleton", Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &pig = field.spawn("minecraft:pig", Vector3f(0.5f, 64.0f, 10.5f));
        require(skeleton.setTarget(field.mOwner, pig.getRuntimeId()), "target the pig");

        RangedAttackGoal::Settings settings;
        settings.mAttackRadius = 15.0f;
        settings.mIntervalMinTicks = 60;
        settings.mIntervalMaxTicks = 60;
        RangedAttackGoal goal(settings);
        require(goal.canUse(field.mOwner, skeleton), "a skeleton with a target and a bow shoots");
        goal.start(field.mOwner, skeleton);

        std::vector<int32_t> shots;
        size_t arrows = 0;
        for (int32_t tick = 1; tick <= 130; tick++) {
            goal.tick(field.mOwner, skeleton);
            const size_t now = field.all<ArrowActor>().size();
            if (now != arrows)
                shots.push_back(tick);
            arrows = now;
        }
        require(shots.size() == 2 && shots[0] == 62 && shots[1] == 123, "shots on ticks 62 and 123");
    }

    void skeletonArrow() {
        Field field;
        MobActor &skeleton = field.spawn("minecraft:skeleton", Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &pig = field.spawn("minecraft:pig", Vector3f(0.5f, 64.0f, 10.5f));
        require(MobShooter::shootAt(field.mOwner, skeleton, pig), "the skeleton shoots");

        const std::vector<ArrowActor *> arrows = field.all<ArrowActor>();
        require(arrows.size() == 1, "one arrow");
        const ProjectileData &data = arrows.front()->getProjectileData();
        require(data.mImpactDamage == -1 && data.mBaseDamage == 2.0f, "mob_arrow damage scales with speed by 2");
        require(closeTo(speedOf(*arrows.front()), 1.6f, 0.25f), "mob_arrow power 1.6");
        require(arrows.front()->getPosition().z > 1.0f, "spawned a block toward the target");
    }

    void drownedTrident() {
        Field field;
        MobActor &drowned = field.spawn("minecraft:drowned", Vector3f(0.5f, 64.0f, 0.5f));
        drowned.addComponentGroup("minecraft:ranged_mode");
        MobActor &pig = field.spawn("minecraft:pig", Vector3f(0.5f, 64.0f, 8.5f));
        require(MobShooter::shootAt(field.mOwner, drowned, pig), "the drowned throws");

        const std::vector<ThrownTridentActor *> tridents = field.all<ThrownTridentActor>();
        require(tridents.size() == 1, "one trident");
        require(tridents.front()->getProjectileData().mImpactDamage == 8, "thrown_trident impact damage 8");
    }

    // a healthy target out of 8 blocks: the poison entry, aux 25, thrown at the shooter's power 0.75
    void witchPotion() {
        Field field;
        MobActor &witch = field.spawn("minecraft:witch", Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &pig = field.spawn("minecraft:pig", Vector3f(0.5f, 64.0f, 5.5f));
        require(MobShooter::shootAt(field.mOwner, witch, pig), "the witch throws");

        const std::vector<SplashPotionActor *> potions = field.all<SplashPotionActor>();
        require(potions.size() == 1, "one splash potion");
        require(potions.front()->getPotionId() == 25, "poison for a healthy target");
        require(closeTo(speedOf(*potions.front()), 0.75f, 0.01f), "the shooter's power wins");
    }

    void impactDamage() {
        std::mt19937 random(1);
        const std::unique_ptr<json::Value> fixed = json::parse(R"({"damage": {"min": 8, "max": 8}})");
        const std::unique_ptr<json::Value> none = json::parse(R"({"damage": {"min": 0, "max": 0}})");
        const std::unique_ptr<json::Value> number = json::parse(R"({"damage": 6})");
        require(MobShooter::impactDamage(*fixed, random) == 8, "fixed range");
        require(MobShooter::impactDamage(*none, random) == -1, "zero range falls back to speed");
        require(MobShooter::impactDamage(*number, random) == 6, "plain number");
    }

    void vanillaDefinitions() {
        EntityDefinitions::initialize();

        const json::Value *arrow = EntityDefinitions::find("minecraft:arrow");
        const json::Value &mobArrow = path(arrow, {"component_groups", "minecraft:mob_arrow", "minecraft:projectile"});
        require(closeTo((float) mobArrow.get("power")->number(), 1.6f, 0.0001f), "mob arrow power");
        require(mobArrow.get("uncertainty_base")->integer() == 16, "mob arrow uncertainty base");

        const json::Value &stray = path(EntityDefinitions::find("minecraft:stray"),
                                        {"components", "minecraft:shooter", "aux_val"});
        require(stray.integer() == 19, "stray shoots slowness arrows");

        const json::Value &bogged = path(
                EntityDefinitions::find("minecraft:bogged"),
                {"component_groups", "minecraft:ranged_attack", "minecraft:shooter", "aux_val"});
        require(bogged.integer() == 26, "bogged shoots poison arrows");

        const json::Value &pillager = path(
                EntityDefinitions::find("minecraft:pillager"),
                {"component_groups", "minecraft:ranged_attack", "minecraft:behavior.ranged_attack", "attack_radius"});
        require(pillager.integer() == 8, "pillager attack radius");
    }
}

int main() {
    try {
        aimPitch();
        uncertainty();
        vanillaDefinitions();
        impactDamage();
        skeletonArrow();
        drownedTrident();
        witchPotion();
        attackTimeline();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "ranged mob tests passed" << std::endl;
    return 0;
}
