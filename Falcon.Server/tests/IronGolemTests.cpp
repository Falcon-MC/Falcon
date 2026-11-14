#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/MoveTowardsTargetGoal.h"
#include "World/Actor/AI/Goal/OfferFlowerGoal.h"
#include "World/Actor/AI/Goal/RandomGroundPosition.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Village/Village.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Iron golem", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, "mob did not spawn");
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }
    };

    void attackDamage(Arena &arena) {
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(0.5f, 100.0f, 0.5f));
        bool lowest = false;
        bool highest = false;
        for (int32_t roll = 0; roll < 4000; ++roll) {
            const float damage = golem.getAttackDamage(Difficulty::Normal);
            require(damage >= 7.0f && damage <= 21.0f, "iron golem damage left its 7 to 21 range");
            lowest = lowest || damage == 7.0f;
            highest = highest || damage == 21.0f;
        }
        require(lowest && highest, "iron golem damage never reached an end of its range");
        require(golem.getAttackDamage(Difficulty::Peaceful) == 0.0f, "iron golem hurts on peaceful");
    }

    void ingotRepair(Arena &arena) {
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(4.5f, 100.0f, 0.5f));
        ServerPlayer &player = arena.player(9301, Vector3f(5.5f, 100.0f, 0.5f));

        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:iron_ingot", 2));
        require(!golem.onInteract(arena.mOwner, player), "a healthy golem took the ingot");
        require(player.getInventory().getItemInHand().mCount == 2, "a healthy golem used the ingot");

        golem.setHealth(50.0f);
        require(golem.onInteract(arena.mOwner, player), "the ingot did not repair the golem");
        require(golem.getHealth() == 75.0f, "the ingot did not restore 25 health");
        require(player.getInventory().getItemInHand().mCount == 1, "the repair did not use the ingot");

        golem.setHealth(90.0f);
        require(golem.onInteract(arena.mOwner, player), "the last ingot did not repair the golem");
        require(golem.getHealth() == 100.0f, "the repair overflowed the maximum health");
    }

    void groundSnap(Arena &arena) {
        Level &level = *arena.mLevel;
        for (int32_t x = -20; x <= -10; ++x) {
            for (int32_t z = -5; z <= 5; ++z)
                level.setBlockState(x, 60, z, BlockState("minecraft:stone"));
        }

        Vector3i ground;
        require(RandomGroundPosition::snapToGround(level, Vector3i(-15, 60, 0), 55, 65, ground)
                        && ground.y == 61,
                "a point inside the floor did not snap on top of it");
        require(RandomGroundPosition::snapToGround(level, Vector3i(-15, 64, 0), 57, 67, ground) && ground.y == 61,
                "a point above the floor did not fall onto it");
        require(!RandomGroundPosition::snapToGround(level, Vector3i(-15, 70, 0), 66, 74, ground),
                "a point out of vertical reach found the floor");
    }

    void moveTowardsTarget(Arena &arena) {
        Level &level = *arena.mLevel;
        // a floor wide enough for every point the search can draw towards +x
        for (int32_t x = 8; x <= 47; ++x) {
            for (int32_t z = -16; z <= 16; ++z)
                level.setBlockState(x, 70, z, BlockState("minecraft:stone"));
        }
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(10.5f, 71.0f, 0.5f));
        MobActor &skeleton = arena.spawn("minecraft:skeleton", Vector3f(36.5f, 71.0f, 0.5f));
        require(golem.setTarget(arena.mOwner, skeleton.getRuntimeId()), "could not aim the golem");

        MoveTowardsTargetGoal goal(0.25f, 32.0f);
        require(goal.canUse(arena.mOwner, golem), "a target 26 blocks away was not chased");
        skeleton.setPosition(Vector3f(43.5f, 71.0f, 0.5f));
        require(!goal.canUse(arena.mOwner, golem), "a target 33 blocks away was chased");
    }

    void offerFlower(Arena &arena) {
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(0.5f, 100.0f, 30.5f));
        OfferFlowerGoal::Settings settings;
        settings.mChanceToStart = 1.0f;
        settings.mDurationTicks = 400;
        settings.mSearchArea = Vector3f(6.0f, 2.0f, 6.0f);
        OfferFlowerGoal goal(settings);
        require(!goal.canUse(arena.mOwner, golem), "a flower was offered with nobody to take it");

        MobActor &child = arena.spawn("minecraft:villager_v2", Vector3f(4.5f, 100.0f, 30.5f));
        child.addComponentGroup("baby");
        require(goal.canUse(arena.mOwner, golem), "the child next to the golem was not offered a flower");
        goal.start(arena.mOwner, golem);
        require(goal.canContinueToUse(arena.mOwner, golem), "the offer ended at once");
        goal.stop(arena.mOwner, golem);

        child.setPosition(Vector3f(12.5f, 100.0f, 30.5f));
        require(!goal.canUse(arena.mOwner, golem), "a child beyond the search area was offered a flower");
    }

    void villageAggression(Arena &arena) {
        Village village;
        MobActor &golem = arena.spawn("minecraft:iron_golem", Vector3f(0.5f, 100.0f, -30.5f));
        MobActor &skeleton = arena.spawn("minecraft:skeleton", Vector3f(10.5f, 100.0f, -30.5f));
        ServerPlayer &player = arena.player(9401, Vector3f(4.5f, 100.0f, -30.5f));

        require(village.findClosestAggressor(arena.mOwner, golem) == nullptr, "a quiet village had an aggressor");
        village.addAggressor(skeleton.getRuntimeId(), 1000);
        require(village.findClosestAggressor(arena.mOwner, golem) == &skeleton, "the aggressor was forgotten");
        village.addAggressor(player.getRuntimeId(), 1000);
        require(village.findClosestAggressor(arena.mOwner, golem) == &player, "the closer aggressor lost");

        // hitting a villager costs 1 standing: the golem only turns on a player at -15
        for (int32_t hit = 0; hit < 14; ++hit)
            village.changeStanding(player.getUniqueId(), -1);
        require(village.findClosestHostilePlayer(arena.mOwner, golem) == nullptr,
                "-14 standing made the village hostile");
        village.changeStanding(player.getUniqueId(), -1);
        require(village.findClosestHostilePlayer(arena.mOwner, golem) == &player,
                "-15 standing left the village calm");

        village.changeStanding(player.getUniqueId(), -100);
        require(village.getStanding(player.getUniqueId()) == -30, "standing went below -30");
        village.changeStanding(player.getUniqueId(), 100);
        require(village.getStanding(player.getUniqueId()) == 10, "standing went above 10");

        // remembered for 300 ticks after the hit
        village.tick(arena.mOwner, *arena.mLevel, 1300);
        require(village.findClosestAggressor(arena.mOwner, golem) == &player, "the aggressor left before 300 ticks");
        village.tick(arena.mOwner, *arena.mLevel, 1301);
        require(village.findClosestAggressor(arena.mOwner, golem) == nullptr, "the aggressor stayed past 300 ticks");

        // a negative standing climbs back by 1 every 12000 ticks
        village.changeStanding(player.getUniqueId(), -25);
        village.tick(arena.mOwner, *arena.mLevel, 1320);
        require(village.getStanding(player.getUniqueId()) == -14, "the first decay pass did not raise the standing");
        village.tick(arena.mOwner, *arena.mLevel, 1350);
        require(village.getStanding(player.getUniqueId()) == -14, "the standing rose before 12000 ticks");
        village.tick(arena.mOwner, *arena.mLevel, 13320);
        require(village.getStanding(player.getUniqueId()) == -13, "the standing did not rise after 12000 ticks");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-iron-golem-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        attackDamage(arena);
        ingotRepair(arena);
        groundSnap(arena);
        moveTowardsTarget(arena);
        offerFlower(arena);
        villageAggression(arena);
        std::cout << "iron golem checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
