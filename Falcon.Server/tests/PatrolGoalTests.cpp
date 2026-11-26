#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/FollowTargetLeaderGoal.h"
#include "World/Actor/AI/Goal/HoldGroundGoal.h"
#include "World/Actor/AI/Goal/MoveToRandomBlockGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    const char *const PILLAGER = "minecraft:pillager";
    const char *const RANGED_GROUP = "minecraft:ranged_attack";

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Patrol goals", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "patrol.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }
            for (int32_t x = -32; x < 48; x++) {
                for (int32_t z = -32; z < 48; z++)
                    mLevel->setBlockState(x, 99, z, BlockState("minecraft:stone"));
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event = nullptr) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, "mob did not spawn");
            if (event != nullptr)
                mob->fireEvent(mOwner, event);
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

    HoldGroundGoal::Settings patrolHoldGround() {
        HoldGroundGoal::Settings settings;
        settings.mMinRadius = 10.0f;
        settings.mBroadcast = true;
        settings.mBroadcastRange = 8.0f;
        settings.mWithinRadiusEvent =
                std::shared_ptr<json::Value>(json::parse("{\"event\":\"minecraft:ranged_mode\",\"target\":\"self\"}"));
        return settings;
    }

    void onHurtTriggers(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &pillager = arena.spawn(PILLAGER, Vector3f(0.5f, 100.0f, 0.5f), "minecraft:spawn_as_patrol_follower");
        pillager.fireEvent(arena.mOwner, "minecraft:calm");
        require(!pillager.hasComponentGroup(RANGED_GROUP), "calm kept the ranged group");

        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(2.5f, 100.0f, 0.5f));
        arena.mOwner.damageActor(pillager, 1.0f, &zombie);
        require(pillager.hasComponentGroup(RANGED_GROUP), "on_hurt did not fire ranged_mode");
        require(zombie.getLastHurtMob() == pillager.getRuntimeId(), "the zombie's hit was not remembered");
    }

    void holdGround(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &pillager = arena.spawn(PILLAGER, Vector3f(0.5f, 100.0f, 0.5f), "minecraft:spawn_as_patrol_follower");
        MobActor &friendly = arena.spawn(PILLAGER, Vector3f(4.5f, 100.0f, 0.5f), "minecraft:spawn_as_patrol_follower");
        ServerPlayer &player = arena.player(9201, Vector3f(20.5f, 100.0f, 0.5f));
        require(pillager.setTarget(arena.mOwner, player.getRuntimeId()), "target refused");

        HoldGroundGoal goal(patrolHoldGround());
        require(goal.canUse(arena.mOwner, pillager), "a far target was not held");
        goal.start(arena.mOwner, pillager);
        require(friendly.getTarget(arena.mOwner) == &player, "the target was not broadcast");

        pillager.fireEvent(arena.mOwner, "minecraft:calm");
        friendly.fireEvent(arena.mOwner, "minecraft:calm");
        player.setPosition(Vector3f(6.5f, 100.0f, 0.5f));
        require(!goal.canContinueToUse(arena.mOwner, pillager), "a close target was still held");
        goal.stop(arena.mOwner, pillager);
        require(pillager.hasComponentGroup(RANGED_GROUP) && friendly.hasComponentGroup(RANGED_GROUP),
                "within_radius_event did not reach the patrol");

        // a target that already hit the pillager is left to the attack goals
        player.setPosition(Vector3f(20.5f, 100.0f, 0.5f));
        arena.mOwner.damageActor(pillager, 1.0f, &player);
        require(!goal.canUse(arena.mOwner, pillager), "held ground against its attacker");
    }

    void followCaptain(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &captain = arena.spawn(PILLAGER, Vector3f(10.5f, 100.0f, 0.5f), "minecraft:promote_to_patrol_captain");
        MobActor &follower = arena.spawn(PILLAGER, Vector3f(0.5f, 100.0f, 0.5f), "minecraft:spawn_as_patrol_follower");

        FollowTargetLeaderGoal::Settings settings;
        settings.mSpeed = 0.28f;
        settings.mWithinRadius = 64.0f;
        settings.mFollowDistance = 5.0f;
        settings.mSearchCooldownTicks = 1;
        settings.mLeaderFilters = std::shared_ptr<json::Value>(json::parse(
                "{\"test\":\"has_component\",\"subject\":\"other\",\"value\":\"minecraft:is_illager_captain\"}"));
        FollowTargetLeaderGoal goal(settings);
        require(goal.canUse(arena.mOwner, follower), "the follower did not take its captain");
        require(follower.getTargetCaptainId() == captain.getUniqueId(), "the captain was not kept on the follower");

        follower.setPosition(Vector3f(8.5f, 100.0f, 0.5f));
        require(!goal.canUse(arena.mOwner, follower), "followed inside follow_distance");

        Tag saved = follower.saveNbt();
        MobActor &reloaded = arena.spawn(PILLAGER, Vector3f(0.5f, 100.0f, 4.5f));
        reloaded.loadNbt(saved);
        require(reloaded.getTargetCaptainId() == captain.getUniqueId(), "TargetCaptainID was not saved");
    }

    void moveToRandomBlock(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &captain = arena.spawn(PILLAGER, Vector3f(0.5f, 100.0f, 0.5f), "minecraft:promote_to_patrol_captain");
        MoveToRandomBlockGoal goal(0.2f, 0.5f, 16);
        bool picked = false;
        for (int32_t attempt = 0; attempt < 20 && !picked; ++attempt)
            picked = goal.canUse(arena.mOwner, captain);
        require(picked, "no random block was picked on open ground");
        goal.start(arena.mOwner, captain);
        goal.tick(arena.mOwner, captain);
        require(!captain.getNavigation().isDone(), "the march did not start");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-patrol-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        onHurtTriggers(directory);
        holdGround(directory);
        followCaptain(directory);
        moveToRandomBlock(directory);
        std::cout << "patrol goal checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
