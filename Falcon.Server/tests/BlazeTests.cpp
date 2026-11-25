#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
    const int32_t TICKS_PER_SECOND = 20;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Blazes", "", 1};
        Level &mLevel;

        explicit Arena(DimensionType dimension) : mLevel(mOwner.getWorlds().getDefault().getLevel(dimension)) {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -8; x <= 8; x++) {
                for (int32_t z = -8; z <= 8; z++)
                    mLevel.setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        void ticks(int32_t count) {
            // the world tick moves with the overworld, as a server tick does
            for (int32_t tick = 0; tick < count; tick++) {
                mOwner.getWorlds().getDefault().getOverworld().tickTime();
                // the server tick stays put without a full tick: a reservation for another tick frees the path budget
                PathFinder::tryReserveSearch(-1);
                mOwner.tickActors();
            }
        }

        size_t countOf(const char *identifier) {
            size_t found = 0;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    ++found;
            }
            return found;
        }

        MobActor *first(const char *identifier) {
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    return dynamic_cast<MobActor *>(entry.second.get());
            }
            return nullptr;
        }
    };

    // ranged_mode: burst_shots 3, burst_interval 0.3 s
    void volley() {
        Arena arena(DimensionType::Nether);
        arena.player(1, Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &blaze = arena.spawn("minecraft:blaze", Vector3f(0.5f, 64.0f, 12.5f), "minecraft:entity_spawned");
        require(blaze.hasComponentGroup("ranged_mode") || blaze.hasComponentGroup("mode_switcher"),
                "the blaze spawned without its attack modes");

        std::unordered_set<int64_t> seen;
        std::vector<int32_t> shots;
        for (int32_t tick = 0; tick < 12 * TICKS_PER_SECOND && shots.size() < 3; tick++) {
            arena.ticks(1);
            for (auto &entry: arena.mOwner.getActors()) {
                if (entry.second->getTypeId() == "minecraft:small_fireball" && seen.insert(entry.first).second)
                    shots.push_back(tick);
            }
        }
        require(shots.size() == 3, "the blaze did not fire a volley of three");
        require(shots[1] - shots[0] == 6 && shots[2] - shots[1] == 6, "the volley's shots are not 6 ticks apart");
    }

    // hurt_on_condition: 1 drowning damage a tick in contact with water
    void water() {
        Arena arena(DimensionType::Nether);
        MobActor &blaze = arena.spawn("minecraft:blaze", Vector3f(0.5f, 64.0f, 0.5f), "minecraft:entity_spawned");
        arena.mLevel.setBlockState(0, 64, 0, BlockState("minecraft:water"));
        const float before = blaze.getHealth();
        arena.ticks(1);
        require(before - blaze.getHealth() == 1.0f, "water did not hurt the blaze by 1");
    }

    void snowball() {
        Arena arena(DimensionType::Nether);
        MobActor &blaze = arena.spawn("minecraft:blaze", Vector3f(0.5f, 64.0f, 0.5f), "minecraft:entity_spawned");
        MobActor &skeleton = arena.spawn("minecraft:wither_skeleton", Vector3f(4.5f, 64.0f, 0.5f), "minecraft:entity_spawned");
        auto *ball = dynamic_cast<SnowballActor *>(
                arena.mOwner.spawnActor(arena.mLevel, "minecraft:snowball", Vector3f(0.5f, 65.0f, 1.5f)));
        require(ball != nullptr, "no snowball");
        const float blazeBefore = blaze.getHealth();
        ball->onHitActor(arena.mOwner, blaze.getPosition(), blaze);
        require(blazeBefore - blaze.getHealth() == 3.0f, "a snowball did not deal 3 to the blaze");
        const float skeletonBefore = skeleton.getHealth();
        ball->onHitActor(arena.mOwner, skeleton.getPosition(), skeleton);
        require(skeleton.getHealth() == skeletonBefore, "a snowball hurt a mob other than a blaze");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        volley();
        water();
        snowball();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "blaze tests passed" << std::endl;
    return 0;
}
