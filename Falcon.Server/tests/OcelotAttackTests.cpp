#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/OcelotAttackGoal.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"OcelotAttack", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();

        Arena() {
            require(mLevel.openStorage(ServerPaths::getRoot().string()), "could not open ocelot attack storage");
            for (int x = -1; x <= 1; ++x) {
                for (int z = -1; z <= 1; ++z)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int x = -16; x <= 16; ++x) {
                for (int z = -4; z <= 4; ++z)
                    mLevel.setBlockState(x, 99, z, VanillaBlocks::STONE().toBlockState());
            }
        }

        MobActor &spawn(const char *identifier, const char *event, const Vector3f &position) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) {
                        static_cast<MobActor &>(actor).setSpawnEvent(event);
                    }));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }
    };

    // the bundled definition's values
    OcelotAttackGoal::Settings settingsFor(const MobActor &mob) {
        const json::Value *component = mob.getComponent("minecraft:behavior.ocelotattack");
        require(component != nullptr, "the mob has no ocelotattack");
        const float base = mob.getMovementSpeed();
        OcelotAttackGoal::Settings settings;
        settings.mCoolDownTicks = (int32_t) (component->get("cooldown_time")->number(0.0) * 20.0);
        settings.mMaxDistance = (float) component->get("max_distance")->number(0.0);
        settings.mMaxSneakRange = (float) component->get("max_sneak_range")->number(0.0);
        settings.mMaxSprintRange = (float) component->get("max_sprint_range")->number(0.0);
        settings.mReachMultiplier = (float) component->get("reach_multiplier")->number(0.0);
        settings.mWalkSpeed = base * (float) component->get("walk_speed_multiplier")->number(0.0);
        settings.mSneakSpeed = base * (float) component->get("sneak_speed_multiplier")->number(0.0);
        settings.mSprintSpeed = base * (float) component->get("sprint_speed_multiplier")->number(0.0);
        return settings;
    }

    bool sneaking(const MobActor &mob) {
        return mob.getFlags().get(ActorFlag::Sneaking);
    }

    bool sprinting(const MobActor &mob) {
        return mob.getFlags().get(ActorFlag::Sprinting);
    }

    // a wild cat stalks a rabbit crouched, sprints at it from four blocks and strikes once a second
    void catPouncesOnRabbit() {
        Arena arena;
        MobActor &cat = arena.spawn("minecraft:cat", "minecraft:spawn_wild_adult", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &rabbit = arena.spawn("minecraft:rabbit", "minecraft:entity_spawned", Vector3f(10.5f, 100.0f, 0.5f));
        const OcelotAttackGoal::Settings settings = settingsFor(cat);
        require(settings.mCoolDownTicks == 20 && settings.mMaxSprintRange == 4.0f, "unexpected cat attack values");

        OcelotAttackGoal goal(settings);
        require(!goal.canUse(arena.mOwner, cat), "the cat attacked with no target");
        require(cat.setTarget(arena.mOwner, rabbit.getRuntimeId()), "the cat could not target the rabbit");
        require(goal.canUse(arena.mOwner, cat), "the cat did not go for its target");
        goal.start(arena.mOwner, cat);

        goal.tick(arena.mOwner, cat);
        require(sneaking(cat) && !sprinting(cat), "the cat did not stalk its target crouched");
        require(!cat.getNavigation().isDone(), "the cat did not walk to its target");

        rabbit.setPosition(Vector3f(3.5f, 100.0f, 0.5f));
        goal.tick(arena.mOwner, cat);
        require(sprinting(cat) && !sneaking(cat), "the cat did not sprint at its close target");

        // cat width 0.48 times 2: reach 0.96 blocks
        const float health = rabbit.getHealth();
        rabbit.setPosition(Vector3f(1.2f, 100.0f, 0.5f));
        goal.tick(arena.mOwner, cat);
        require(rabbit.getHealth() < health, "the cat did not strike its target in reach");

        const float struck = rabbit.getHealth();
        for (int tick = 0; tick < settings.mCoolDownTicks - 1 && rabbit.isAlive(); ++tick)
            goal.tick(arena.mOwner, cat);
        require(rabbit.getHealth() == struck, "the cat struck again before its cooldown");

        rabbit.setPosition(Vector3f(15.6f, 100.0f, 0.5f));
        require(!goal.canContinueToUse(arena.mOwner, cat), "the cat chased a target past 15 blocks");
        goal.stop(arena.mOwner, cat);
        require(cat.getTarget(arena.mOwner) == nullptr, "the cat kept the target it gave up on");
        require(!sneaking(cat) && !sprinting(cat), "the cat kept its stalking pose");
    }

    // the ocelot carries the same values
    void ocelotHasTheAttack() {
        Arena arena;
        MobActor &ocelot = arena.spawn("minecraft:ocelot", "minecraft:entity_spawned", Vector3f(0.5f, 100.0f, 0.5f));
        const OcelotAttackGoal::Settings settings = settingsFor(ocelot);
        require(settings.mMaxDistance == 15.0f && settings.mReachMultiplier == 2.0f
                        && settings.mSneakSpeed < settings.mWalkSpeed && settings.mWalkSpeed < settings.mSprintSpeed,
                "unexpected ocelot attack values");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-ocelot-attack-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        catPouncesOnRabbit();
        ocelotHasTheAttack();
        std::cout << "ocelot attack checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
